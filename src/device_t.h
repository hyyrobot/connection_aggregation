#ifndef DEVICE_T_H
#define DEVICE_T_H

#include "fd_guard_t.h"

#include <netinet/in.h>

namespace autolabor::connection_aggregation
{
    struct device_t
    {
        device_t(const char *, int);
        ~device_t();

        void bind(uint16_t) const;

        size_t send(const msghdr *) const;
        size_t receive(msghdr *) const;

    private:
        fd_guard_t _socket;
        int _epoll;
    };

} // namespace autolabor::connection_aggregation

#endif // DEVICE_T_H