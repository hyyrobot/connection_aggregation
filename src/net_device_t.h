#ifndef NET_DEVICE_T_H
#define NET_DEVICE_T_H

#include <netinet/in.h>
#include <linux/if.h>

#include <vector>

#include "fd_guard_t.h"

namespace autolabor::connection_aggregation
{
    struct net_device_t
    {
        net_device_t(const char *);

        const char *name() const;

        bool push_address(in_addr);
        bool erase_address(in_addr);
        in_addr address() const;

        size_t send(const msghdr *) const;

    private:
        char _name[IFNAMSIZ];
        std::vector<in_addr_t> _addresses;
        fd_guard_t _socket;
    };

} // namespace autolabor::connection_aggregation

#endif // NET_DEVICE_T_H
