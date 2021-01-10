#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/epoll.h>
#include <unistd.h>

uint16_t static checksum(const void *data, size_t n)
{
    auto p = reinterpret_cast<const uint16_t *>(data);
    uint32_t sum = 0;

    for (; n > 1; n -= 2)
        sum += *p++;

    if (n)
        sum += *p;

    p = reinterpret_cast<uint16_t *>(&sum);
    sum = p[0] + p[1];
    sum = p[0] + p[1];

    return ~p[0];
}

std::ostream &operator<<(std::ostream &out, in_addr a)
{
    char text[16];
    inet_ntop(AF_INET, &a, text, sizeof(text));
    return out << text;
}

namespace autolabor::connection_aggregation
{
    void host_t::read_tun(uint8_t *buffer, size_t size)
    {
        auto n = read(_tun, buffer, size);
        if (n == 0)
            return;
        // 来自网络层的报文不可能不包含一个头
        if (n < sizeof(ip))
            THROW_ERRNO(__FILE__, __LINE__, "receive from tun");
        auto ip_ = reinterpret_cast<ip *>(buffer);
        // 丢弃非 ip v4 包
        if (ip_->ip_v != 4)
            return;
        // 回环包直接写回去
        if (ip_->ip_dst.s_addr == _address.s_addr)
            auto _ = write(_tun, buffer, n);
        // 重写协议头
        auto header = reinterpret_cast<aggregator_t *>(buffer);
        header->type1 = {.type0 = FORWARD, .ttl = MAX_TTL};
        header->data[0] = ip_->ip_p;
        // header->data[1:2] = ip_->ip_off;
        header->id1 = ++_id_s ? _id_s : ++_id_s;
        auto p = _remotes.find(ip_->ip_dst.s_addr);
        // 发送
        if (p == _remotes.end())
        { // 未知路由，大广播
            header->id0 = 0;
            for (auto &[a, _] : _remotes)
                send_strand({a}, buffer + sizeof(in_addr), n - sizeof(in_addr));
        }
        else
        { // 已知路由，束广播
            header->id0 = p->second.exchange(header->id1);
            send_strand(ip_->ip_dst, buffer + sizeof(in_addr), n - sizeof(in_addr));
        }
    }

    host_t::forward_t host_t::read_from_device(device_index_t index, uint8_t *const buffer, const size_t size)
    {
        sockaddr_in remote{.sin_family = AF_INET};
        iovec iov{.iov_base = buffer, .iov_len = size};
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = &iov,
            .msg_iovlen = 1,
        };
        auto n = _devices.at(index).receive(&msg);

        connection_key_union _union{.pair{.src_index = index, .dst_port = remote.sin_port}};

#define HEADER reinterpret_cast<aggregator_t *>
        add_remote_inner(HEADER(buffer)->source, remote.sin_addr, remote.sin_port);

        auto &r = _remotes.at(HEADER(buffer)->source.s_addr);
        r.received_once(_union, HEADER(buffer)->type1.state);

        // 空包仅用于更新状态
        if (n < sizeof(in_addr) + sizeof(aggregator_t))
        {
            send_void(HEADER(buffer)->source, true);
            return {};
        }
        // 有一个完整的聚合协议头
        // 提取路由信息
        auto origin = HEADER(buffer)->ip_src;
        auto &o = _remotes.try_emplace(origin.s_addr).first->second;
        // 如果经过中转，更新路由表
        if (auto distance = MAX_TTL - HEADER(buffer)->type1.ttl)
            o.add_route(HEADER(buffer)->source, distance);
        // 如果包含路由请求，即时响应
        if (HEADER(buffer)->type1.type0 == FORWARD && !HEADER(buffer)->id0)
        {
            auto dst = reinterpret_cast<ip *>(buffer)->ip_dst;
            auto p = _remotes.find(dst.s_addr);
            if (p != _remotes.end())
            {
                // 构造反馈包
                aggregator_t msg{
                    .type1{
                        .type0 = SPECIAL,
                        .ttl = static_cast<uint8_t>(MAX_TTL - p->second.distance() - 1),
                    },
                    .ip_src = dst,
                };
                // 原路返回
                send_single(r.sending(_union), reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
            }
        }
        send_void(HEADER(buffer)->source, true);
        return {HEADER(buffer)->type1.type0 == FORWARD ? origin : in_addr{}, static_cast<uint32_t>(n)};
    }

    void host_t::receive(uint8_t *const buffer, const size_t size)
    {
        // 只能在一个线程调用，拿不到锁则放弃
        std::unique_lock<std::mutex> L(_receiving, std::try_to_lock);
        if (!L.owns_lock())
            return;
        send_list_request();

        epoll_event events[8];
        while (true)
        {
            // 阻塞
            auto event_count = epoll_wait(_epoll, events, sizeof(events) / sizeof(epoll_event), 10);
            forward_t received{};
            for (auto ei = 0; ei < event_count; ++ei)
            {
                switch (events[ei].data.u32)
                {
                case ID_TUN:
                    read_tun(buffer, size);
                    continue;
                case ID_UNIX:
                    read_unix(buffer, size);
                    continue;
                case ID_NETLINK:
                    read_netlink(buffer, size);
                    continue;
                default:
                    received = read_from_device(static_cast<device_index_t>(events[ei].data.u32), buffer, size);
                    break;
                }
            }
            for (auto &[a, r] : _remotes)
            {
                auto focus = received.origin.s_addr == a;
                auto vectors = focus ? r.exchange(buffer, received.size)
                                     : r.exchange(nullptr, 0);
                for (auto &v : vectors)
                {
                    auto d = v.data();
                    auto s = v.size();
                    if (!s)
                    {
                        d = buffer;
                        s = received.size;
                    }

#define IP reinterpret_cast<ip *>
                    // 如果目的是本机
                    if (IP(d)->ip_dst.s_addr == _address.s_addr)
                    {
                        // 恢复 ip 包
                        IP(d)->ip_v = 4;
                        IP(d)->ip_hl = 5;
                        IP(d)->ip_tos = 0;
                        IP(d)->ip_len = htons(s);
                        IP(d)->ip_ttl = HEADER(d)->type1.ttl;
                        IP(d)->ip_p = HEADER(d)->data[0];
                        IP(d)->ip_sum = 0;
                        IP(d)->ip_sum = checksum(d, sizeof(ip));
                        // 上传
                        if (s != write(_tun, d, s))
                            THROW_ERRNO(__FILE__, __LINE__, "forward msg to tun")
                    }
                    else if (--HEADER(d)->type1.ttl)
                    {
                        auto p = _remotes.find(IP(d)->ip_dst.s_addr);
                        // 发送
                        if (p == _remotes.end())
                            // 未知路由，继续向来源以外的方向广播
                            for (auto &[a, _] : _remotes)
                            {
                                if (a != HEADER(d)->source.s_addr && a != HEADER(d)->ip_src.s_addr)
                                    send_strand({a}, d + sizeof(in_addr), s - sizeof(in_addr));
                            }
                        else
                            // 已知路由，束广播
                            send_strand(IP(d)->ip_dst, d + sizeof(in_addr), s - sizeof(in_addr));
                    }
#undef IP
#undef HEADER
                }
            }
        }
    }

} // namespace autolabor::connection_aggregation