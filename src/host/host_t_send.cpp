#include "../host_t.h"

namespace autolabor::connection_aggregation
{
    size_t host_t::send_single(in_addr dst, connection_key_union _union, bool forward, const uint8_t *buffer, size_t size)
    {
        if (!forward)
        {
            sockaddr_in remote{
                .sin_family = AF_INET,
                .sin_port = _union.pair.dst_port,
            };
            pack_type_t byte;
            {
                READ_LOCK(_srand_mutex);
                auto &s = _srands.at(dst.s_addr);
                {
                    read_lock l(s.port_mutex);
                    remote.sin_addr = s.ports.at(remote.sin_port);
                }
                {
                    read_lock l(s.connection_mutex);
                    byte.state = s.connections.at(_union.key).state();
                }
            }
            iovec iov[]{
                {.iov_base = &_address, .iov_len = sizeof(_address)},
                {.iov_base = &byte, .iov_len = sizeof(byte)},
            };
            msghdr msg{
                .msg_name = &remote,
                .msg_namelen = sizeof(remote),
                .msg_iov = iov,
                .msg_iovlen = sizeof(iov) / sizeof(iovec),
            };
            {
                READ_LOCK(_device_mutex);
                return _devices.at(_union.pair.src_index).send(&msg);
            }
        }
        else
        {
        }

        return 0;
    }

} // namespace autolabor::connection_aggregation