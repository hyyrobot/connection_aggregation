#include "../host_t.h"

namespace autolabor::connection_aggregation
{
    void host_t::add_remote(in_addr remote_host, uint16_t port, in_addr address)
    {
        {
            WRITE_GRAUD(_srand_mutex);
            auto &srand = _srands.try_emplace(remote_host.s_addr).first->second;
            if (!srand.remote_detected(port, address))
                return;
            READ_GRAUD(_device_mutex);
            srand.add_connections(port, _devices);
        }
    }

} // namespace autolabor::connection_aggregation