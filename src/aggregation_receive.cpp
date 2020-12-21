#include "net_devices_t.h"

#include <netpacket/packet.h>

namespace autolabor::connection_aggregation
{
    bool net_devices_t::receive(ip *header, uint8_t *buffer, size_t size)
    {
        sockaddr_ll remote;
        struct
        {
            char host[14];
            uint8_t id, protocol;
        } extra;
        iovec iov[]{
            {.iov_base = header, .iov_len = sizeof(ip)},
            {.iov_base = &extra, .iov_len = sizeof(extra)},
            {.iov_base = buffer, .iov_len = size},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };
        if (recvmsg(_receiver, &msg, 0) <= 0 || header->ip_p != IPPROTO_MINE)
            return false;

        header->ip_hl -= 4;
        header->ip_len = ntohs(header->ip_len) - 16;
        header->ip_p = extra.protocol;
        header->ip_dst = _tun_address;
        auto [_, success] = _remotes1.try_emplace(header->ip_src.s_addr, extra.host);
        if (success)
        {
            auto p = _remotes2.find(extra.host);
            if (p == _remotes2.end())
                _remotes2[extra.host] = {header->ip_src.s_addr};
            else
                p->second.insert(header->ip_src.s_addr);
        }
        return true;
    }

} // namespace autolabor::connection_aggregation