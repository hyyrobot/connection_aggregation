#include "device_t.h"

#include "ERRNO_MACRO.h"

namespace autolabor::connection_aggregation
{
    device_t::device_t(const char *name)
        : _socket(socket(AF_INET, SOCK_DGRAM, 0))
    {
        // 绑定套接字到网卡
        if (setsockopt(_socket, SOL_SOCKET, SO_BINDTODEVICE, name, std::strlen(name)))
            THROW_ERRNO(__FILE__, __LINE__, "bind socket to device " << name);
    }

    void device_t::bind(uint16_t port) const
    {
        sockaddr_in a{
            .sin_family = AF_INET,
            .sin_port = port,
        };
        // 绑定套接字到端口
        if (::bind(_socket, reinterpret_cast<sockaddr *>(&a), sizeof(a)))
            THROW_ERRNO(__FILE__, __LINE__, "bind socket to port " << port)
    }

    size_t device_t::send(const msghdr *msg) const
    {
        return sendmsg(_socket, msg, MSG_WAITALL);
    }
} // namespace autolabor::connection_aggregation