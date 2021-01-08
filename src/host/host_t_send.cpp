#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <netinet/ip.h>
#include <unistd.h>

#include <iostream>

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
        constexpr static uint8_t nothing = 0;
        // 未指定具体目标，向所有邻居发送握手
        if (!dst.s_addr)
            for (auto &[d, r] : _remotes)
                for (auto k : r.filter_for_handshake(on_demand))
                    send_single({d}, {k}, &nothing, 1);
        // 向指定的邻居发送握手
        else
        {
            auto p = _remotes.find(dst.s_addr);
            if (p != _remotes.end())
                for (auto k : p->second.filter_for_handshake(on_demand))
                    send_single(dst, {k}, &nothing, 1);
        }
    }

    bool host_t::send_single(
        in_addr dst,
        connection_key_union _union,
        const uint8_t *buffer,
        size_t size)
    {
        auto type = *reinterpret_cast<const pack_type_t *>(buffer);
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_port = _union.pair.dst_port,
        };
        // 获取目的地址和连接状态
        auto &r = _remotes.at(dst.s_addr);
        r.set_address_and_state(_union, &remote.sin_addr, &type);

        iovec iov[]{
            {.iov_base = &_address, .iov_len = sizeof(in_addr)},
            {.iov_base = &type, .iov_len = 1},
            {.iov_base = (void *)(buffer + 1), .iov_len = size - 1},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };

        auto result = _devices.at(_union.pair.src_index).send(&msg) == sizeof(in_addr) + size;
        if (result)
            r.sent_once(_union);
        else
            THROW_ERRNO(__FILE__, __LINE__, "send msg from " << _union.pair.src_index);
        return result;
    }

    size_t host_t::send_strand(in_addr dst, const uint8_t *buffer, size_t size)
    {
        auto &r = _remotes.at(dst.s_addr);
        // 查询路由表
        auto next = r.next();
        if (next.s_addr)
            r = _remotes.at(next.s_addr);
        else
            next = dst;
        // 发送
        auto keys = r.filter_for_forward();
        if (keys.size() > 2)
        {
            std::vector<std::future<bool>> results(keys.size());
            std::transform(keys.begin(), keys.end(), results.begin(),
                           [&](auto k) {
                               return _threads.submit([=, this] {
                                   return send_single(next, {k}, buffer, size);
                               });
                           });
            for (auto &f : results)
                f.wait();
        }
        else
            for (auto k : keys)
                send_single(next, {k}, buffer, size);
        return keys.size();
    }

    void host_t::send(in_addr dst, uint8_t *buffer, size_t size, in_addr exclude)
    {
        auto type = reinterpret_cast<pack_type_t *>(buffer);
        auto p = _remotes.find(dst.s_addr);
        if (type->ask_route = (p == _remotes.end()))
            // 未知路由，大广播
            for (auto &[a, _] : _remotes)
            {
                if (a != exclude.s_addr)
                    send_strand({a}, buffer, size);
            }
        else
            // 已知路由，束广播
            send_strand(dst, buffer, size);
    }

} // namespace autolabor::connection_aggregation