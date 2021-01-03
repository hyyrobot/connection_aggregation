#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <netinet/ip.h>
#include <unistd.h>

namespace autolabor::connection_aggregation
{
    size_t host_t::send_handshake(in_addr dst, bool on_demand)
    {
        std::vector<connection_key_t> keys;
        {
            READ_LOCK(_srand_mutex);
            auto &s = _srands.at(dst.s_addr);
            read_lock l(s.connection_mutex);
            if (on_demand)
            { // 按需发送握手
                for (auto &[k, c] : s.connections)
                    if (c.need_handshake())
                        keys.push_back(k);
            }
            else
            { // 向所有连接发送握手
                keys.reserve(s.connections.size());
                for (auto &[k, _] : s.connections)
                    keys.push_back(k);
            }
        }
        uint8_t nothing = 0;
        for (auto k : keys)
            send_single(dst, {.key = k}, &nothing, 1);
        return keys.size();
    }

    uint16_t srand_t::get_id()
    {
        return _id++;
    }

    std::vector<connection_key_t> srand_t::take_best_connections() const
    {
        std::vector<connection_key_t> result;
        auto max = 0;
        read_lock l(connection_mutex);
        for (const auto &[k, c] : connections)
        {
            auto s_ = c.state();
            if (s_ < max)
                continue;
            if (s_ > max)
            {
                max = s_;
                result.resize(1);
                result[0] = k;
            }
            else
                result.push_back(k);
        }
        return result;
    }

    void host_t::forward(uint8_t *buffer, size_t size)
    {
        // 只能在一个线程调用，拿不到锁则放弃
        TRY_LOCK(_forwarding, return );
        while (true)
        {
            auto n = read(_tun, buffer, size);
            if (n == 0)
                continue;
            if (n < 20)
                THROW_ERRNO(__FILE__, __LINE__, "receive from tun");
            auto ip_ = reinterpret_cast<ip *>(buffer);
            // 丢弃非 ip v4 包
            if (ip_->ip_v != 4)
                continue;
            // 回环包直接写回去
            if (ip_->ip_dst.s_addr == _address.s_addr)
                write(_tun, buffer, n);
            // 转发
            forward_inner(buffer, n);
        }
    }

    void host_t::forward_inner(uint8_t *buffer, size_t n)
    {
        constexpr static pack_type_t TYPE{.multiple = true, .forward = true};
        constexpr static auto PAYLOAD_OFFSET = 4;
        // 额外信息压入 ip 头部
        // 报头
        //       4       |       4       |       4       |       4       |       4       |       4       |       4       |       4       |
        //    version    | header length |        type of service        |                        total length                           |
        //                              id                               |                             offset                            |
        //              ttl              |            protocol           |                            check sum                          |
        // 改为
        //       4       |       4       |       4       |       4       |       4       |       4       |       4       |       4       |
        //                                                         direct source                                                         |
        //           pack type           |            nothing            |                             offset                            |
        //              ttl              |            protocol           |                         id for srand                          |
        auto ip_ = reinterpret_cast<ip *>(buffer);
        auto dst = ip_->ip_dst;
        { // 检查路由表
            READ_LOCK(_route_mutex);
            auto p = _route.find(dst.s_addr);
            // 路由表没有，丢弃
            if (p == _route.end())
                return;
            // 如果不是直连，发往直连路由
            auto q = p->second;
            if (q.length)
                dst = q.next;
        }
        std::vector<connection_key_t> keys;
        {
            READ_LOCK(_srand_mutex);
            auto &s = _srands.at(dst.s_addr);
            ip_->ip_sum = s.get_id();
            keys = s.take_best_connections();
        }
        *reinterpret_cast<pack_type_t *>(buffer + PAYLOAD_OFFSET) = TYPE;
        for (auto k : keys)
            send_single(dst, {.key = k}, buffer + PAYLOAD_OFFSET, n - PAYLOAD_OFFSET);
    }

    size_t host_t::send_single(in_addr dst, connection_key_union _union, uint8_t *buffer, size_t size)
    {
        auto type = reinterpret_cast<pack_type_t *>(buffer);
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_port = _union.pair.dst_port,
        };
        { // 获取目的地址和连接状态
            READ_LOCK(_srand_mutex);
            auto &s = _srands.at(dst.s_addr);
            {
                read_lock l(s.port_mutex);
                remote.sin_addr = s.ports.at(remote.sin_port);
            }
            {
                read_lock l(s.connection_mutex);
                type->state = s.connections.at(_union.key).state();
            }
        }

        iovec iov[]{
            {.iov_base = &_address, .iov_len = sizeof(_address)},
            {.iov_base = (void *)buffer, .iov_len = size},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };

        size_t result;
        {
            READ_LOCK(_device_mutex);
            result = _devices.at(_union.pair.src_index).send(&msg);
        }
        if (result > 0)
        {
            READ_LOCK(_srand_mutex);
            auto &s = _srands.at(dst.s_addr);
            read_lock l(s.connection_mutex);
            s.connections.at(_union.key).sent_once();
        }
        else
            THROW_ERRNO(__FILE__, __LINE__, "send msg from " << _union.pair.src_index);
        return result;
    }

} // namespace autolabor::connection_aggregation