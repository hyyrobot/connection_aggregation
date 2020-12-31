#include "../host_t.h"

#include <vector>

namespace autolabor::connection_aggregation
{
    size_t host_t::send_handshake(in_addr dst)
    {
        std::vector<connection_key_t> keys;
        {
            READ_LOCK(_srand_mutex);
            auto &s = _srands.at(dst.s_addr);
            {
                read_lock l(s.connection_mutex);
                for (auto &[k, c] : s.connections)
                    if (c.need_handshake())
                        keys.push_back(k);
            }
        }
        for (auto k : keys)
            send_single(dst, {.key = k}, {});
        return keys.size();
    }

    size_t host_t::send_single(in_addr dst, connection_key_union _union, pack_type_t type, const uint8_t *buffer, size_t size)
    {
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
                type.state = s.connections.at(_union.key).state();
            }
        }

        iovec iov[]{
            {.iov_base = &_address, .iov_len = sizeof(_address)},
            {.iov_base = &type, .iov_len = sizeof(type)},
            {.iov_base = (void *)buffer, .iov_len = size},
        };
        constexpr static auto len = sizeof(iov) / sizeof(iovec);
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = buffer && size ? len : len - 1,
        };

        {
            READ_LOCK(_device_mutex);
            return _devices.at(_union.pair.src_index).send(&msg);
        }
    }

} // namespace autolabor::connection_aggregation