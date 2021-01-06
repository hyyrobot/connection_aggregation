#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <netinet/ip.h>
#include <unistd.h>

namespace autolabor::connection_aggregation
{
    void host_t::yell(in_addr dst)
    {
        fd_guard_t temp = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (!dst.s_addr)
        {
            constexpr static uint8_t msg = YELL;
            sendto(temp, &msg, sizeof(msg), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));
        }
        else
        {
            uint8_t msg[5]{SEND_HANDSHAKE};
            *reinterpret_cast<in_addr *>(msg + 1) = dst;
            sendto(temp, &msg, sizeof(msg), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));
        }
    }

    void host_t::send_void(in_addr dst, bool on_demand)
    {
        uint8_t nothing = 0;
        // 未指定具体目标，向所有邻居发送握手
        if (!dst.s_addr)
            for (auto &[d, s] : _srands)
                for (auto k : s.filter_for_handshake(on_demand))
                    send_single({d}, {.key = k}, &nothing, 1);
        // 向指定的邻居发送握手
        else
        {
            auto p = _srands.find(dst.s_addr);
            if (p != _srands.end())
                for (auto k : p->second.filter_for_handshake(on_demand))
                    send_single(dst, {.key = k}, &nothing, 1);
        }
    }

    uint16_t srand_t::get_id()
    {
        return _id++;
    }

    std::vector<connection_key_t> srand_t::filter_for_forward() const
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

    std::vector<connection_key_t> srand_t::filter_for_handshake(bool on_demand) const
    {
        std::vector<connection_key_t> result;
        read_lock l(connection_mutex);
        if (on_demand)
        { // 按需发送握手
            for (auto &[k, c] : connections)
                if (c.need_handshake())
                    result.push_back(k);
        }
        else
        { // 向所有连接发送握手
            result.reserve(connections.size());
            for (auto &[k, _] : connections)
                result.push_back(k);
        }
        return result;
    }

    void host_t::forward(uint8_t *buffer, size_t n)
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
            if (q.distance)
                dst = q.next;
        }

        auto &s = _srands.at(dst.s_addr);
        ip_->ip_sum = s.get_id();
        ip_->ip_ttl = MAX_TTL;
        *reinterpret_cast<pack_type_t *>(buffer + PAYLOAD_OFFSET) = TYPE;

        for (auto k : s.filter_for_forward())
            send_single(dst, {.key = k}, buffer + PAYLOAD_OFFSET, n - PAYLOAD_OFFSET);
    }

    size_t host_t::send_single(in_addr dst, connection_key_union _union, uint8_t *buffer, size_t size)
    {
        auto type = reinterpret_cast<pack_type_t *>(buffer);
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_port = _union.pair.dst_port,
        };
        // 获取目的地址和连接状态
        auto &s = _srands.at(dst.s_addr);
        {
            read_lock l(s.port_mutex);
            remote.sin_addr = s.ports.at(remote.sin_port);
        }
        {
            read_lock l(s.connection_mutex);
            type->state = s.connections.at(_union.key).state();
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
            auto &s = _srands.at(dst.s_addr);
            read_lock l(s.connection_mutex);
            s.connections.at(_union.key).sent_once();
        }
        else
            THROW_ERRNO(__FILE__, __LINE__, "send msg from " << _union.pair.src_index);
        return result;
    }

} // namespace autolabor::connection_aggregation