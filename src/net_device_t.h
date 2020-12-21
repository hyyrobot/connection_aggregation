#include "fd_guard_t.h"

#include <netinet/ip.h>
#include <linux/if.h>

#include <vector>

namespace autolabor::connection_aggregation
{
    struct net_device_t
    {
        net_device_t(const char *, const char *);

        const char *name() const;

        bool push_address(in_addr);
        bool erase_address(in_addr);
        in_addr address() const;

        size_t send_to(const uint8_t*, size_t, in_addr, unsigned) const;

    private:
        char _name[IFNAMSIZ];
        std::vector<in_addr_t> _addresses;
        fd_guard_t _socket;

        mutable ip _header;
        mutable struct
        {
            char host[13];
            uint8_t protocol;
            uint16_t id;
        } _extra;

        mutable sockaddr_in _remote;
        mutable iovec _iov[3];
        msghdr _msg;
    };

} // namespace autolabor::connection_aggregation
