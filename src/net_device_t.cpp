#include "net_device_t.h"

#include <algorithm>
#include <cstring>

namespace autolabor::connection_aggregation
{
    constexpr static auto on = 1;

    net_device_t::net_device_t(const char *name)
        : _socket(socket(AF_INET, SOCK_RAW, SOCK_RAW))
    {
        // 指定协议栈不再向发出的分组添加 ip 头
        setsockopt(_socket, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
        // 绑定套接字到网卡
        setsockopt(_socket, SOL_SOCKET, SO_BINDTODEVICE, name, std::strlen(name));

        std::strcpy(_name, name);
    }

    const char *net_device_t::name() const
    {
        return _name;
    }

    bool net_device_t::push_address(in_addr address)
    {
        auto p = std::find(_addresses.begin(), _addresses.end(), address.s_addr);
        if (p != _addresses.end())
            return false;

        _addresses.push_back(address.s_addr);
        return true;
    }

    bool net_device_t::erase_address(in_addr address)
    {
        auto p = std::find(_addresses.begin(), _addresses.end(), address.s_addr);
        if (p != _addresses.end())
            return false;

        _addresses.erase(p);
        return true;
    }

    in_addr net_device_t::address() const
    {
        return _addresses.empty() ? in_addr{} : in_addr{_addresses.front()};
    }

    size_t net_device_t::addresses_size() const
    {
        return _addresses.size();
    }

    size_t net_device_t::send(const msghdr *msg) const
    {
        return sendmsg(_socket, msg, 0);
    }

} // namespace autolabor::connection_aggregation