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
        // 多播唯一 id 写到原 sum 位置，重置 ttl
        ip_->ip_sum = _id_s++;
        ip_->ip_ttl = MAX_TTL;
        *reinterpret_cast<pack_type_t *>(&ip_->ip_id) = {.multiple = true, .forward = true};
        // 发送
        send(ip_->ip_dst, buffer + sizeof(in_addr), n - sizeof(in_addr), {});
    }

    void host_t::read_from_device(device_index_t index, uint8_t *buffer, size_t size)
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

        const auto SOURCE = reinterpret_cast<const in_addr *>(buffer);       // 解出源虚拟地址
        const auto TYPE = reinterpret_cast<const pack_type_t *>(SOURCE + 1); // 解出类型字节

        const auto source = *SOURCE;
        const auto type = *TYPE;

        add_remote_inner(source, remote.sin_addr, remote.sin_port);

        auto &r = _remotes.at(source.s_addr);
        r.received_once(_union, type.state);

        if (type.forward)
        {
            // 转发包中有一个 ip 头
            auto ip_ = reinterpret_cast<ip *>(buffer);
            if (ip_->ip_src.s_addr == _address.s_addr)
                return;
            // 如果经过中转，更新路由表
            if (auto distance = MAX_TTL - ip_->ip_ttl)
                add_route(ip_->ip_src, source, distance);
            //
            auto &o = _remotes.at(ip_->ip_src.s_addr);

            // 用于去重的连接束序号存在 sum 处
            if (!type.multiple || o.check_unique(ip_->ip_sum))
            {
                // 如果目的是本机
                if (ip_->ip_dst.s_addr == _address.s_addr)
                {
                    // 恢复 ip 包
                    ip_->ip_v = 4;
                    ip_->ip_hl = 5;
                    ip_->ip_tos = 0;
                    ip_->ip_len = htons(n);
                    ip_->ip_sum = 0;
                    ip_->ip_sum = checksum(ip_, sizeof(ip));
                    // 转发
                    if (n != write(_tun, buffer, n))
                        THROW_ERRNO(__FILE__, __LINE__, "forward msg to tun")
                }
                else
                {
                    // 如果跳数没有归零，转发
                    if (--ip_->ip_ttl)
                        send(ip_->ip_dst, buffer + sizeof(in_addr), n - sizeof(in_addr), source);
                    // 如果包含路由请求
                    if (type.ask_route)
                    {
                        // 查询路由表
                        auto p = _remotes.find(ip_->ip_dst.s_addr);
                        if (p != _remotes.end())
                        {
                            msg_route_t msg{
                                .type{.multiple = true, .special = true},
                                .distance = p->second.distance(),
                                .id = _id_s++,
                                .which = ip_->ip_dst,
                            };
                            send_strand(source, reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
                        }
                    }
                }
            }
        }
        else if (type.special && n >= 8 && (!type.multiple || r.check_unique(reinterpret_cast<uint16_t *>(buffer)[3])))
        {
            // 目前只有一种特殊功能包
            if (n == sizeof(msg_route_t) + sizeof(in_addr))
            {
                auto msg = reinterpret_cast<const msg_route_t *>(TYPE);
                add_route(msg->which, source, msg->distance + 1);
            }
        };
        send_void(source, true);
    }

    void host_t::receive(uint8_t *buffer, size_t size)
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
            auto event_count = epoll_wait(_epoll, events, sizeof(events) / sizeof(epoll_event), -1);
            if (event_count == 0)
                continue;
            if (event_count < 0)
                THROW_ERRNO(__FILE__, __LINE__, "wait epoll");
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
                    read_from_device(static_cast<device_index_t>(events[ei].data.u32), buffer, size);
                    break;
                }
            }
        }
    }

} // namespace autolabor::connection_aggregation