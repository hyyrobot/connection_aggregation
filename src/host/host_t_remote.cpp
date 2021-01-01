#include "../host_t.h"

namespace autolabor::connection_aggregation
{
    void host_t::add_remote(in_addr remote_host, uint16_t port, in_addr address)
    {
        {
            WRITE_LOCK(_srand_mutex);
            _srands.try_emplace(remote_host.s_addr).first;
        }
        {
            READ_LOCK(_srand_mutex);
            auto &s = _srands.at(remote_host.s_addr);
            {
                write_lock lp(s.port_mutex);
                auto [q, b] = s.ports.try_emplace(port, address);
                if (b)
                {
                    READ_LOCK(_device_mutex);
                    write_lock lc(s.connection_mutex);
                    connection_key_union _union{.pair{.dst_port = port}};
                    for (const auto &[i, _] : _devices)
                    {
                        _union.pair.src_index = i;
                        s.connections.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(_union.key),
                            std::forward_as_tuple());
                    }
                }
                else
                    q->second = address;
            }
        }
    }

} // namespace autolabor::connection_aggregation