#include "../program_t.h"

#include <netinet/ip.h>

namespace autolabor::connection_aggregation
{
    size_t program_t::send_single(uint8_t *payload, size_t size, in_addr dst, uint64_t connection)
    {
        constexpr static uint16_t header_size = sizeof(ip) + sizeof(common_extra_t);
        common_extra_t extra{
            .host = _tun.address(),
            .connection = connection,
        };
        ip header{
            .ip_hl = header_size / 4,
            .ip_v = 4,

            .ip_tos = 0,
            .ip_len = static_cast<uint16_t>(header_size + size),
            .ip_ttl = 64,
            .ip_p = IPPROTO_MINE,
        };
        connection_key_union key{.key = extra.connection};
        {
            READ_GRAUD(_connection_mutex);
            header.ip_off = _connections[dst.s_addr][extra.connection].next_id();
        }

        {
            READ_GRAUD(_remote_mutex);
            header.ip_dst = _remotes[dst.s_addr][key.dst_index];
        }

        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_addr = header.ip_dst,
        };
        iovec iov[]{
            {.iov_base = &header, .iov_len = sizeof(header)},
            {.iov_base = (void *)&extra, .iov_len = sizeof(extra)},
            {.iov_base = payload, .iov_len = size},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };
        READ_GRAUD(_local_mutex);
        const auto &d = _devices.at(key.src_index);
        header.ip_src = d.address();
        return d.send(&msg);
    }

} // namespace autolabor::connection_aggregation
