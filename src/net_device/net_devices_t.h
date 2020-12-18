#include "../common/fd_guard_t.h"

#include <netinet/in.h>
#include <linux/if.h>

#include <unordered_map>
#include <ostream>

namespace autolabor::connection_aggregation
{
    struct net_device_t
    {
        char name[IFNAMSIZ];
        std::unordered_map<in_addr_t, uint8_t> addresses;
        fd_guard_t in, out;
    };

    struct net_devices_t
    {
        net_devices_t();

    private:
        fd_guard_t tun;
        char tun_name[IFNAMSIZ];
        unsigned tun_index;
        in_addr_t tun_address;

        fd_guard_t netlink;
        std::unordered_map<unsigned, net_device_t> devices;
    };
} // namespace autolabor::connection_aggregation
