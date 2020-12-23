#include "program_t.h"

#include <arpa/inet.h>

namespace autolabor::connection_aggregation
{
    void program_t::address_added(unsigned index, const char *name, in_addr address)
    {
        GUARD(_netlink_mutex);
        auto [p, _] = _devices.try_emplace(index, name);
        p->second.push_address(address);
    }

    void program_t::address_removed(unsigned index, in_addr address)
    {
        GUARD(_netlink_mutex);
        auto p = _devices.find(index);
        if (p->second.erase_address(address) && p->second.addresses_size() == 0)
            _devices.erase(p);
    }

    std::ostream &operator<<(std::ostream &out, const program_t &program)
    {
        out << "- tun device: " << program._tun.name() << '(' << program._tun.index() << ')' << std::endl
            << "- local devices:" << std::endl;
        char text[IFNAMSIZ];
        for (const auto &device : program._devices)
        {
            auto address = device.second.address();
            inet_ntop(AF_INET, &address, text, sizeof(text));
            out << "  - " << device.second.name() << '(' << device.first << "): " << text << std::endl;
        }
        return out;
    }

} // namespace autolabor::connection_aggregation