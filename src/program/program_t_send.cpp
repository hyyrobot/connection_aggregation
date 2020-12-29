#include "../program_t.h"

#include "../protocol.h"

#include <netinet/ip.h>
#include <vector>

namespace autolabor::connection_aggregation
{
    size_t program_t::send_handshake(in_addr dst, connection_key_t key)
    {
        // `send_single` 会修改这块内存，不可以 const
        extra_t nothing{};
        iovec iov[]{
            {},
            {.iov_base = (void *)&nothing, .iov_len = sizeof(nothing)},
        };
        constexpr static auto iov_len = sizeof(iov) / sizeof(iovec);
        // 如果指定从哪里发送，直接发送
        if (key)
            return send_single(dst, key, iov, iov_len);
        // 否则向所有没完成握手的连接发送握手
        std::vector<connection_key_t> v;
        {
            READ_GRAUD(_connection_mutex);
            for (const auto &[k, c] : _connections.at(dst.s_addr).items)
                if (c.state() < 2)
                    v.emplace_back(k);
        }
        return std::count_if(v.begin(), v.end(), [&](auto k) { return send_single(dst, k, iov, iov_len); }) == v.size();
    }

    size_t program_t::forward(in_addr dst, const uint8_t *buffer, size_t size)
    {
        // `send_single` 会修改这块内存，不可以 const
        extra_t extra{.forward = true};
        iovec iov[]{
            {},
            {.iov_base = (void *)&extra, .iov_len = sizeof(extra)},
            {.iov_base = (void *)buffer, .iov_len = size},
        };
        constexpr static auto iov_len = sizeof(iov) / sizeof(iovec);
        // 检查所有连接，只通过最好的连接转发
        std::vector<connection_key_t> v[3];
        auto max = 0;
        {
            READ_GRAUD(_connection_mutex);
            auto &srand = _connections.at(dst.s_addr);
            extra.id = srand.get_id();
            for (const auto &[k, c] : srand.items)
            {
                auto s = c.state();
                if (s > max)
                    v[max = s].push_back(k);
                else if (s == max)
                    v[s].push_back(k);
            }
        }
        return std::count_if(v[max].begin(), v[max].end(), [&](auto k) { return send_single(dst, k, iov, iov_len); });
    }

    bool program_t::send_single(in_addr dst, connection_key_t key, iovec *iov, size_t count)
    {
        constexpr static uint16_t header_size = sizeof(ip) + sizeof(ip_extra_t);
        // 构造首部
        connection_key_union connection{.key = key};
        ip_extra_t extra{
            .host = _tun.address(),
            .src_index = connection.src_index,
            .dst_index = connection.dst_index,
        };
        // 填写物理网络中的目的地址
        // 远程网卡只增不减，此处只读，因此不用上锁
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_port = 9999,
            .sin_addr = _remotes.at(dst.s_addr).at(connection.dst_index),
        };
        iov[0] = {.iov_base = &extra, .iov_len = sizeof(extra)};
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = count,
        };
        // 填写源地址，发送
        READ_GRAUD(_local_mutex);
        if (_devices.at(connection.src_index).send(&msg))
        {
            // 此处已经锁了 local，connection 移除需要先获得 local，所以此处不用上锁
            _connections.at(dst.s_addr).items.at(key).sent_once();
            return true;
        }
        return false;
    }

} // namespace autolabor::connection_aggregation
