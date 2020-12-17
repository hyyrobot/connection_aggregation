#include <errno.h>
#include <unistd.h>
#include <memory.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/rtnetlink.h>

#include <iostream>

#include "common/fd_guard_t.h"
using namespace autolabor::connection_aggregation;

#define NAMEOF(X) #X

void take_attributes(const rtattr **attributes, const rtattr *p, size_t l)
{
    for (; RTA_OK(p, l); p = RTA_NEXT(p, l))
        if (p->rta_type <= IFLA_MAX)
            attributes[p->rta_type] = p;
        else
            std::cerr << "rta_type out of range( <<" << p->rta_type << ")." << std::endl;
}

int main()
{
    fd_guard_t fd(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE));
    sockaddr_nl local =
        {
            .nl_family = AF_NETLINK,
            .nl_pad = 0,
            .nl_pid = static_cast<unsigned>(getpid()),
            .nl_groups =
                RTMGRP_LINK |
                RTMGRP_IPV4_IFADDR |
                RTMGRP_IPV6_IFADDR |
                RTMGRP_IPV4_ROUTE |
                RTMGRP_IPV6_ROUTE,
        };

    if (bind(fd, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0)
    {
        std::cerr << "Failed to bind netlink socket: " << strerror(errno) << std::endl;
        return 1;
    }

    uint8_t buf[8192]; // message buffer
    iovec iov =        // message structure
        {
            .iov_base = buf,        // set message buffer as io
            .iov_len = sizeof(buf), // set size
        };

    msghdr msg =
        {
            .msg_name = &local,           // local address
            .msg_namelen = sizeof(local), // address size
            .msg_iov = &iov,              // io vector
            .msg_iovlen = 1,              // io size
        };

    while (true)
    {
        auto pack_size = recvmsg(fd, &msg, 0);

        if (pack_size < 0)
        {
            std::cerr << "Failed to read netlink: " << strerror(errno) << std::endl;
            continue;
        }

        if (msg.msg_namelen != sizeof(local))
        {
            std::cerr << "Invalid length of the sender address struct." << std::endl;
            continue;
        }

        size_t length;
        for (auto h = reinterpret_cast<const nlmsghdr *>(buf); NLMSG_OK(h, pack_size); h = NLMSG_NEXT(h, length))
        {
            length = h->nlmsg_len;
            std::string type;
            enum
            {
                LINK,
                ADDRESS,
                ROUTE,
                ELSE,
            } what;

            switch (h->nlmsg_type)
            {
            case RTM_GETLINK:
                type = NAMEOF(RTM_GETLINK);
                what = LINK;
                break;
            case RTM_NEWLINK:
                type = NAMEOF(RTM_NEWLINK);
                what = LINK;
                break;
            case RTM_DELLINK:
                type = NAMEOF(RTM_DELLINK);
                what = LINK;
                break;

            case RTM_GETADDR:
                type = NAMEOF(RTM_GETADDR);
                what = ADDRESS;
                break;
            case RTM_NEWADDR:
                type = NAMEOF(RTM_NEWADDR);
                what = ADDRESS;
                break;
            case RTM_DELADDR:
                type = NAMEOF(RTM_DELADDR);
                what = ADDRESS;
                break;

            case RTM_GETROUTE:
                type = NAMEOF(RTM_GETROUTE);
                what = ROUTE;
                break;
            case RTM_NEWROUTE:
                type = NAMEOF(RTM_NEWROUTE);
                what = ROUTE;
                break;
            case RTM_DELROUTE:
                type = NAMEOF(RTM_DELROUTE);
                what = ROUTE;
                break;

            default:
                what = ELSE;
                break;
            }

            switch (what)
            {
            case LINK:
            {
                ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(h);
                const rtattr *attributes[IFLA_MAX + 1]{};
                take_attributes(attributes, IFLA_RTA(ifi), length - ((uint8_t *)IFLA_RTA(ifi) - (uint8_t *)h));

                auto name = attributes[IFLA_IFNAME] ? (const char *)RTA_DATA(attributes[IFLA_IFNAME]) : "untitled";
                std::cout << '[' << type << "] [" << name << '(' << ifi->ifi_index << ")] [" << ((ifi->ifi_flags & IFF_UP) ? "UP" : "DOWN") << ']' << std::endl;
            }
            break;
            case ADDRESS:
            {
                ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(h);
                const rtattr *attributes[IFA_MAX + 1]{};
                take_attributes(attributes, IFA_RTA(ifa), length - ((uint8_t *)IFA_RTA(ifa) - (uint8_t *)h));

                const char *ifName = attributes[IFLA_IFNAME] ? (char *)RTA_DATA(attributes[IFLA_IFNAME]) : "unnamed";
                char ifAddress[128];
                if (attributes[IFA_LOCAL])
                    inet_ntop(ifa->ifa_family, RTA_DATA(attributes[IFA_LOCAL]), ifAddress, sizeof(ifAddress));
                std::cout << '[' << type << "] [" << ifName << "] [" << ifAddress << '/' << +ifa->ifa_prefixlen << ']' << std::endl;
            }
            break;
            case ROUTE:
            {
                std::cout << '[' << type << "]" << std::endl;
            }
            break;
            default:
                std::cout << "ELSE";
                break;
            }
        }
    }

    return 0;
}