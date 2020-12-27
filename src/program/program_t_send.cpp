#include "../program_t.h"

#include "../protocol.h"

#include <netinet/ip.h>
#include <vector>

namespace autolabor::connection_aggregation
{
    bool program_t::send_handshake(in_addr dst, connection_key_t key)
    {
        nothing_t nothing{}; // `send_single` 会修改这块内存，不可以 const
        iovec iov[]{{}, {.iov_base = (void *)&nothing, .iov_len = sizeof(nothing)}};
        if (key)
            return send_single(dst, key, iov, sizeof(iov) / sizeof(iovec));
        else
        {
            std::vector<connection_key_t> v;
            { // 读取所有连接
                READ_GRAUD(_connection_mutex);
                for (const auto &[k, c] : _connections.at(dst.s_addr))
                    if (c.state() < 2)
                        v.push_back(k);
            }
            return std::count_if(v.begin(), v.end(), [&](auto k) { return send_single(dst, k, iov, sizeof(iov) / sizeof(iovec)); }) == v.size();
        }
    }

    size_t program_t::forward(in_addr dst, const ip *header, const uint8_t *buffer, size_t size)
    {
        constexpr static auto size_except_payload = sizeof(ip) + sizeof(common_extra_t) + sizeof(forward_t);
        // 构造协议
        const forward_t extra{
            .type{.forward = true},
            .protocol = header->ip_p,
            .offset = header->ip_off,
            .src = _tun.address(),
            .dst = dst,
        };
        iovec iov[]{
            {},
            {.iov_base = (void *)&extra, .iov_len = sizeof(extra)},
            {.iov_base = (void *)buffer, .iov_len = size},
        };
        std::vector<connection_key_t> v;
        { // 读取所有可用的连接
            READ_GRAUD(_connection_mutex);
            for (const auto &[k, c] : _connections.at(dst.s_addr))
                if (c.state() > 0)
                    v.push_back(k);
        }
        // 发送
        return std::count_if(v.begin(), v.end(), [&](auto k) { return send_single(dst, k, iov, sizeof(iov) / sizeof(iovec)); });
    }

    bool program_t::send_single(in_addr dst, connection_key_t key, iovec *iov, size_t count)
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
            auto &c = _connections.at(dst.s_addr).at(key);
            reinterpret_cast<pack_type_t *>(iov[1].iov_base)->state = c.state();
            header.basic.ip_id = c.get_id();
        }
        // 填写物理网络中的目的地址
        // 远程网卡只增不减，此处只读，因此不用上锁
        header.basic.ip_dst = _remotes.at(dst.s_addr).at(connection.dst_index);
        // 构造消息
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_addr = header.basic.ip_dst,
        };
        iov[0] = {.iov_base = &header, .iov_len = sizeof(header)};
        for (auto i = 1; i < count; ++i)
            header.basic.ip_len += iov[i].iov_len;
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = count,
        };
        // 填写源地址，发送
        READ_GRAUD(_local_mutex);
        const auto &d = _devices.at(connection.src_index);
        header.basic.ip_src = d.address();
        if (header.basic.ip_len == d.send(&msg))
        {
            // 此处已经锁了 local，connection 移除需要先获得 local，所以此处不用上锁
            _connections.at(dst.s_addr).at(key).sent_once();
            return true;
        }
        return false;
    }

} // namespace autolabor::connection_aggregation
