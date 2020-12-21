#include "net_device_t.h"

#include <netinet/ip.h>
#include <unordered_set>
#include <unordered_map>

namespace autolabor::connection_aggregation
{
    constexpr static auto IPPROTO_MINE = 3;

    struct actual_msg_t
    {
        in_addr remote;
        uint8_t protocol;
        unsigned id;

        uint8_t *buffer;
        size_t size;
    };

    struct net_devices_t
    {
        net_devices_t(const char *, in_addr);

        in_addr tun_address() const;

        size_t send(ip, uint8_t, const uint8_t *) const;
        bool receive(ip *, uint8_t *, size_t);

        std::string operator[](unsigned) const;

    private:
        // tun 套接字

        fd_guard_t _tun;
        char _tun_name[IFNAMSIZ];
        unsigned _tun_index;
        in_addr _tun_address;

        // netlink 套接字
        fd_guard_t _netlink;

        // 用于接收的 ip 套接字
        fd_guard_t _receiver;

        // 实体网卡
        std::unordered_map<unsigned, net_device_t>
            _devices;

        // 用于发送的缓存

        mutable sockaddr_in _remote;
        mutable struct
        {
            in_addr src, dst;
            char host[14];
            uint8_t id, protocol;
        } _extra;
        mutable iovec _iov[3];
        msghdr _msg;

        std::unordered_map<in_addr_t, std::string>
            _remotes1;
        std::unordered_map<std::string, std::unordered_set<in_addr_t>>
            _remotes2;
    };
} // namespace autolabor::connection_aggregation
