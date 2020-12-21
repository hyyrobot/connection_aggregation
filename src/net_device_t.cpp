#include "net_device_t.h"

#include <algorithm>
#include <cstring>

constexpr static auto IPPROTO_MINE = 3, on = 1;

namespace autolabor::connection_aggregation
{

    net_device_t::net_device_t(const char *host, const char *name)
        : _socket(socket(AF_INET, SOCK_RAW, IPPROTO_MINE)),
          _header{
              .ip_hl = (sizeof(_header) + sizeof(_extra)) / 4,
              .ip_v = 4,
              .ip_ttl = 64,
              .ip_p = IPPROTO_MINE,
          },
          _extra{},
          _iov{
              {.iov_base = &_header, .iov_len = sizeof(_header)},
              {.iov_base = &_extra, .iov_len = sizeof(_extra)},
          },
          _remote{
              .sin_family = AF_INET,
          },
          _msg{
              .msg_name = &_remote,
              .msg_namelen = sizeof(_remote),
              .msg_iov = _iov,
              .msg_iovlen = sizeof(_iov) / sizeof(iovec),
          }
    {
        static_assert((sizeof(_header) + sizeof(_extra)) % 4 == 0);

        // 指定协议栈不再向发出的分组添加 ip 头
        setsockopt(_socket, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
        // 绑定套接字到网卡
        setsockopt(_socket, SOL_SOCKET, SO_BINDTODEVICE, name, std::strlen(name));

        std::strcpy(_name, name);
        std::strcpy(_extra.host, host);
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

        if (_addresses.empty())
            _header.ip_src = address;
        _addresses.push_back(address.s_addr);

        return true;
    }

    bool net_device_t::erase_address(in_addr address)
    {
        auto p = std::find(_addresses.begin(), _addresses.end(), address.s_addr);
        if (p != _addresses.end())
            return false;

        _addresses.erase(p);
        if (_addresses.empty())
            _header.ip_src = {};

        return true;
    }

    in_addr net_device_t::address() const
    {
        return _addresses.empty() ? in_addr{} : in_addr{_addresses.front()};
    }

    size_t net_device_t::send_to(const uint8_t *payload, size_t size, in_addr remote, unsigned id) const
    {
        _header.ip_len = _header.ip_hl + size;
        _header.ip_dst = _remote.sin_addr = remote;
        _extra.id = id;
        _iov[2] = {.iov_base = (void *)payload, .iov_len = size};

        return sendmsg(_socket, &_msg, 0);
    }

} // namespace autolabor::connection_aggregation