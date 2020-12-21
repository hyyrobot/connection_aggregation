#include "net_device_t.h"

#include <netinet/in.h>
#include <linux/if.h>

#include <unordered_map>
#include <ostream>

namespace autolabor::connection_aggregation
{
    struct net_devices_t
    {
        net_devices_t();

        size_t receive(msghdr *) const;
        std::unordered_map<unsigned, size_t> send_to(const uint8_t *, size_t, in_addr, unsigned) const;

        std::string operator[](unsigned) const;

    private:
        // tun 套接字

        fd_guard_t _tun;
        char _tun_name[IFNAMSIZ];
        unsigned _tun_index;
        in_addr_t _tun_address;

        // netlink 套接字

        fd_guard_t _netlink;
        std::unordered_map<unsigned, net_device_t> _devices;

        // 用于接收的 ip 套接字

        fd_guard_t _receiver;
    };
} // namespace autolabor::connection_aggregation
