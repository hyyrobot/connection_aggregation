#include "program_t.h"

#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <unistd.h> // getpid

#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>

namespace autolabor::connection_aggregation
{
    fd_guard_t bind_netlink(uint32_t);
    void send_list_request(const fd_guard_t &);

    program_t::program_t(const char *host, in_addr address)
        : _netlink(bind_netlink(RTMGRP_LINK | RTMGRP_IPV4_IFADDR)),
          _receiver(socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))),
          _tun(_netlink, host, address)
    {
        std::thread([this] { local_monitor(); }).detach();
        send_list_request(_netlink);
    }

    fd_guard_t bind_netlink(uint32_t group)
    {
        fd_guard_t fd(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE));
        const sockaddr_nl local{
            .nl_family = AF_NETLINK,
            .nl_pad = 0,
            .nl_pid = static_cast<unsigned>(getpid()),
            .nl_groups = group,
        };
        if (bind(fd, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) != 0)
        {
            std::stringstream builder;
            builder << "Failed to bind netlink socket: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }
        return fd;
    }

    void send_list_request(const fd_guard_t &netlink)
    {
        sockaddr_nl kernel{.nl_family = AF_NETLINK};
        const struct
        {
            nlmsghdr header;
            rtgenmsg message;
        } request{
            .header{
                .nlmsg_len = sizeof(request),
                .nlmsg_type = RTM_GETADDR,
                .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
                .nlmsg_seq = 0,
                .nlmsg_pid = static_cast<unsigned>(getpid()),
            },
            .message{
                .rtgen_family = AF_UNSPEC,
            },
        };
        if (sendto(netlink, &request, sizeof(request), 0, (const sockaddr *)&kernel, sizeof(kernel)) < 0)
        {
            std::stringstream builder;
            builder << "Failed to ask addresses: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }
    }

} // namespace autolabor::connection_aggregation
