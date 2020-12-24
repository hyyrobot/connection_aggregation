#include "../program_t.h"

#include <netinet/ip.h>
#include <vector>

namespace autolabor::connection_aggregation
{
    bool program_t::send_handshake(in_addr dst, connection_key_t key)
    {
        constexpr static nothing_t nothing{};
        constexpr static iovec iov{.iov_base = (void *)&nothing, .iov_len = sizeof(nothing)};
        constexpr static auto size = sizeof(ip) + sizeof(common_extra_t) + sizeof(nothing_t);
        return size == send_single(dst, key, &iov, 1);
    }

    size_t program_t::forward(in_addr dst, const ip *header, const uint8_t *buffer, size_t size)
    {
        constexpr static auto size_except_payload = sizeof(ip) + sizeof(common_extra_t) + sizeof(forward_t);
        std::vector<connection_key_t> connections;
        { // 读取所有可用的连接
            READ_GRAUD(_connection_mutex);
            for (const auto &[key, _] : _connections.at(dst.s_addr))
                connections.push_back(key);
        }
        // 构造协议
        const forward_t extra{
            .type = 1,
            .protocol = header->ip_p,
            .offset = header->ip_off,
            .dst = dst,
        };
        const iovec iov[]{
            {.iov_base = (void *)&extra, .iov_len = sizeof(extra)},
            {.iov_base = (void *)buffer, .iov_len = size},
        };
        const auto total = size_except_payload + size;
        // 发送
        size_t result = 0;
        for (auto key : connections)
            if (send_single(dst, key, iov, 2) == total)
                ++result;
        return result;
    }

    size_t program_t::send_single(in_addr dst, connection_key_t key, const iovec *payload, size_t count)
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
        connection_key_union connection{.key = key};
        common_extra_t extra{
            .host = _tun.address(),
            .src_index = connection.src_index,
            .dst_index = connection.dst_index,
        };
        { // 填写基于连接的包序号
            READ_GRAUD(_connection_mutex);
            header.ip_id = _connections[dst.s_addr][connection.key].get_id();
        }
        { // 填写物理网络中的目的地址
            READ_GRAUD(_remote_mutex);
            header.ip_dst = _remotes[dst.s_addr][connection.dst_index];
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
            header.ip_len += (iov[i + 2] = payload[i]).iov_len;
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov.data(),
            .msg_iovlen = iov.size(),
        };
        // 填写源地址，发送
        READ_GRAUD(_local_mutex);
        const auto &d = _devices.at(connection.src_index);
        header.ip_src = d.address();
        return d.send(&msg);
    }

} // namespace autolabor::connection_aggregation
