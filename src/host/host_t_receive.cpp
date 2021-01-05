#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <netinet/ip.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <iostream>

namespace autolabor::connection_aggregation
{
    bool srand_t::check_received(uint16_t id)
    {
        constexpr static auto timeout = std::chrono::milliseconds(500);

        std::lock_guard<std::mutex> lock(_id_updater);
        const auto now = std::chrono::steady_clock::now();

        auto p = _received_time.find(id);
        if (p == _received_time.end())
        {
            _received_time.emplace(id, now);
            _received_id.push(id);
        }
        else if (now - p->second > timeout)
        {
            p->second = now;
            while (true)
            {
                auto pop = _received_id.front();
                _received_id.pop();
                if (pop != id)
                    _received_time.erase(pop);
                else
                    break;
            }
        }
        else
            return false;
        while (true)
        {
            auto pop = _received_id.front();
            auto qoq = _received_time.find(pop);
            if (now - qoq->second > timeout)
            {
                _received_id.pop();
                _received_time.erase(qoq);
            }
            else
                break;
        }
        return true;
    }

    uint16_t checksum(const void *data, size_t n)
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

    void host_t::receive(uint8_t *buffer, size_t size)
    {
        // 只能在一个线程调用，拿不到锁则放弃
        TRY_LOCK(_receiving, return );

        auto SOURCE = reinterpret_cast<const in_addr *>(buffer);       // 解出源虚拟地址
        auto TYPE = reinterpret_cast<const pack_type_t *>(SOURCE + 1); // 解出类型字节

        // auto t1 = std::chrono::steady_clock::now();
        epoll_event events[8];
        while (true)
        {
            // 阻塞
            // auto now = std::chrono::steady_clock::now();
            // std::cout << "R " << std::chrono::duration<float, std::micro>(now - t1).count() << "us" << std::endl;
            auto event_count = epoll_wait(_epoll, events, sizeof(events) / sizeof(epoll_event), -1);
            // t1 = std::chrono::steady_clock::now();
            if (event_count == 0)
                continue;
            if (event_count < 0)
                THROW_ERRNO(__FILE__, __LINE__, "wait epoll");
            for (auto ei = 0; ei < event_count; ++ei)
            {
                // 接收
                auto index = static_cast<device_index_t>(events[ei].data.u32);
                sockaddr_in remote{.sin_family = AF_INET};
                iovec iov{.iov_base = buffer, .iov_len = size};
                msghdr msg{
                    .msg_name = &remote,
                    .msg_namelen = sizeof(remote),
                    .msg_iov = &iov,
                    .msg_iovlen = 1,
                };
                size_t n;
                {
                    READ_LOCK(_device_mutex);
                    n = _devices.at(index).receive(&msg);
                }

                connection_key_union _union{.pair{.src_index = index, .dst_port = remote.sin_port}};

                const auto source = *SOURCE;
                const auto type = *TYPE;
                add_remote(source, remote.sin_port, remote.sin_addr);
                {
                    READ_LOCK(_srand_mutex);
                    auto &s = _srands.at(source.s_addr);
                    read_lock lc(s.connection_mutex);
                    s.connections.at(_union.key).received_once(type.state);
                    if (type.forward)
                    {
                        // 转发包中有一个 ip 头
                        auto ip_ = reinterpret_cast<ip *>(buffer);
                        // 用于去重的连接束序号存在 sum 处
                        if (!type.multiple || s.check_received(ip_->ip_sum))
                        {
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
                            else if (--ip_->ip_ttl)
                                forward_inner(buffer, n);
                        }
                    }
                    else if (type.multiple)
                    { // 需要去重
                        if (n >= 4 && s.check_received(reinterpret_cast<uint8_t *>(buffer)[1]))
                            ; // TODO 使用包内容
                    };
                }
                send_handshake(source);
            }
        }
    }
} // namespace autolabor::connection_aggregation