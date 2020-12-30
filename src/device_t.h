#ifndef DEVICE_T_H
#define DEVICE_T_H

#include "fd_guard_t.h"

#include <netinet/in.h>

namespace autolabor::connection_aggregation
{
    struct device_t
    {
        device_t(const char *);

        void bind(uint16_t) const;
        size_t send(const msghdr *) const;

    private:
        fd_guard_t _socket;
    };

} // namespace autolabor::connection_aggregation

#endif // DEVICE_T_H