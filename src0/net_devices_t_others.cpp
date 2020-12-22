#include "net_devices_t.h"

#include <sstream>
#include <netpacket/packet.h>

namespace autolabor::connection_aggregation
{
    in_addr net_devices_t::tun_address() const
    {
        return {_tun_address};
    }

    std::string net_devices_t::operator[](unsigned index) const
    {
        auto p = _devices.find(index);
        if (p == _devices.end())
            return "unknown";

        std::stringstream builder;
        builder << p->second.name() << '[' << index << ']';
        return builder.str();
    }
} // namespace autolabor::connection_aggregation
