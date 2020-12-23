#ifndef TUN_DEVICE_T_H
#define TUN_DEVICE_T_H

#include <netinet/in.h>
#include <linux/if.h>

#include "fd_guard_t.h"

namespace autolabor::connection_aggregation
{
    struct tun_device_t
    {
        tun_device_t(const fd_guard_t &, const char *, in_addr);

        const char *name() const;
        unsigned index() const;

    private:
        fd_guard_t _tun;

        char _name[IFNAMSIZ];
        unsigned _index;
        in_addr _address;
    };

} // namespace autolabor::connection_aggregation

#endif // TUN_DEVICE_T_H
