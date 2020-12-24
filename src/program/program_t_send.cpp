#include "../program_t.h"

#include <netinet/ip.h>
#include <vector>

namespace autolabor::connection_aggregation
{
    size_t program_t::send_single(in_addr dst, connection_key_union::key_t key, const iovec *payload, size_t count)
    {
        constexpr static uint16_t header_size = sizeof(ip) + sizeof(common_extra_t);
        // 构造首部
        ip header{
            .ip_hl = header_size / 4,
            .ip_v = 4,

            .ip_tos = 0,
            .ip_len = header_size,
            .ip_ttl = 64,
            .ip_p = IPPROTO_MINE,
        };
        common_extra_t extra{
            .host = _tun.address(),
            .connection{.key = key},
        };
        { // 填写基于连接的包序号
            READ_GRAUD(_connection_mutex);
            header.ip_id = _connections[dst.s_addr][extra.connection.key].get_id();
        }
        { // 填写物理网络中的目的地址
            READ_GRAUD(_remote_mutex);
            header.ip_dst = _remotes[dst.s_addr][extra.connection.dst_index];
        }
        // 构造消息
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_addr = header.ip_dst,
        };
        std::vector<iovec> iov(count + 2);
        iov[0] = {.iov_base = &header, .iov_len = sizeof(header)};
        iov[1] = {.iov_base = (void *)&extra, .iov_len = sizeof(extra)};
        for (auto i = 0; i < count; ++i)
        {
            header.ip_len += payload[i].iov_len;
            iov[i + 2] = payload[i];
        }
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov.data(),
            .msg_iovlen = iov.size(),
        };
        // 填写源地址，发送
        READ_GRAUD(_local_mutex);
        const auto &d = _devices.at(extra.connection.src_index);
        header.ip_src = d.address();
        return d.send(&msg);
    }

} // namespace autolabor::connection_aggregation
