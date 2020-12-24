#include "../program_t.h"

#include "../protocol.h"

#include <netinet/ip.h>
#include <vector>

namespace autolabor::connection_aggregation
{
    bool program_t::send_handshake(in_addr dst, connection_key_t key)
    {
        constexpr static nothing_t nothing{};
        constexpr static iovec iov{.iov_base = (void *)&nothing, .iov_len = sizeof(nothing)};
        return send_single(dst, key, &iov, 1);
    }

    size_t program_t::forward(in_addr dst, const ip *header, const uint8_t *buffer, size_t size)
    {
        constexpr static auto size_except_payload = sizeof(ip) + sizeof(common_extra_t) + sizeof(forward_t);
        std::vector<connection_key_t> connections;
        // 构造协议
        const forward_t extra{
            .type{.forward = true},
            .protocol = header->ip_p,
            .offset = header->ip_off,
            .dst = dst,
        };
        const iovec iov[]{
            {.iov_base = (void *)&extra, .iov_len = sizeof(extra)},
            {.iov_base = (void *)buffer, .iov_len = size},
        };
        { // 读取所有可用的连接
            READ_GRAUD(_connection_mutex);
            for (const auto &[key, _] : _connections.at(dst.s_addr))
                connections.push_back(key);
        }
        // 发送
        size_t result = 0;
        for (auto key : connections)
            if (send_single(dst, key, iov, 2))
                ++result;
        return result;
    }

    bool program_t::send_single(in_addr dst, connection_key_t key, const iovec *payload, size_t count)
    {
        constexpr static uint16_t header_size = sizeof(ip) + sizeof(common_extra_t);
        // 构造首部
        connection_key_union connection{.key = key};
        struct
        {
            ip basic;
            common_extra_t extra;
        } header{
            .basic{
                .ip_hl = header_size / 4,
                .ip_v = 4,

                .ip_tos = 0,
                .ip_len = header_size,
                .ip_ttl = 64,
                .ip_p = IPPROTO_MINE,
            },
            .extra{
                .host = _tun.address(),
                .src_index = connection.src_index,
                .dst_index = connection.dst_index,
            },
        };
        { // 填写基于连接的包序号
            READ_GRAUD(_connection_mutex);
            auto &c = _connections[dst.s_addr][key];
            reinterpret_cast<pack_type_t *>(payload->iov_base)->state = c.state();
            header.basic.ip_id = c.get_id();
        }
        // 填写物理网络中的目的地址
        // 远程网卡只增不减，此处只读，因此不用上锁
        header.basic.ip_dst = _remotes[dst.s_addr][connection.dst_index];
        // 构造消息
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_addr = header.basic.ip_dst,
        };
        std::vector<iovec> iov(count + 1);
        iov[0] = {.iov_base = &header, .iov_len = sizeof(header)};
        for (auto i = 0; i < count; ++i)
            header.basic.ip_len += (iov[i + 1] = payload[i]).iov_len;
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov.data(),
            .msg_iovlen = iov.size(),
        };
        // 填写源地址，发送
        READ_GRAUD(_local_mutex);
        const auto &d = _devices.at(connection.src_index);
        header.basic.ip_src = d.address();
        if (header.basic.ip_len == d.send(&msg))
        {
            // 此处已经锁了 local，connection 移除需要先获得 local，所以此处不用上锁
            _connections[dst.s_addr][key].sent_once();
            return true;
        }
        return false;
    }

} // namespace autolabor::connection_aggregation
