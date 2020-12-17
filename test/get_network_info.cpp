#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <linux/if.h>

#include <iostream>

#include "../src/common/fd_guard_t.h"

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

    sockaddr_nl
        local{
            .nl_family = AF_NETLINK,
            .nl_pad = 0,
            .nl_pid = static_cast<unsigned>(getpid()),
            .nl_groups = 0,
        },
        kernel{
            .nl_family = AF_NETLINK,
            .nl_pad = 0,
            .nl_pid = 0,
            .nl_groups = 0,
        };

    if (bind(fd, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0)
    {
        std::cerr << "Failed to bind netlink socket: " << strerror(errno) << std::endl;
        exit(1);
    }

    struct
    {
        nlmsghdr header;
        rtgenmsg message;
    } request{
        .header{
            .nlmsg_len = sizeof(request),
            .nlmsg_type = RTM_GETLINK,
            .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
            .nlmsg_seq = 0,
            .nlmsg_pid = static_cast<unsigned>(getpid()),
        },
        .message{
            .rtgen_family = AF_UNSPEC,
        },
    };
    if (sendto(fd, &request, sizeof(request), 0, (const sockaddr *)&kernel, request.header.nlmsg_len) < 0)
    {
        std::cerr << "send failed: " << strerror(errno) << std::endl;
        exit(1);
    }

    bool loop = true;
    uint8_t buf[8192];
    while (loop)
    {
        auto pack_size = read(fd, buf, sizeof(buf));
        if (pack_size < 0)
        {
            std::cerr << "Failed to read netlink: " << strerror(errno) << std::endl;
            continue;
        }

        for (auto h = reinterpret_cast<const nlmsghdr *>(buf); NLMSG_OK(h, pack_size); h = NLMSG_NEXT(h, pack_size))
            switch (h->nlmsg_type)
            {
            case RTM_NEWLINK:
            {
                ifinfomsg *ifi = (ifinfomsg *)NLMSG_DATA(h);
                const rtattr *attributes[IFLA_MAX + 1]{};
                take_attributes(attributes, IFLA_RTA(ifi), h->nlmsg_len - ((uint8_t *)IFLA_RTA(ifi) - (uint8_t *)h));

                auto name = attributes[IFLA_IFNAME] ? (const char *)RTA_DATA(attributes[IFLA_IFNAME]) : "untitled";
                std::cout << '[' << name << '(' << ifi->ifi_index << ")] [" << ((ifi->ifi_flags & IFF_UP) ? "UP" : "DOWN") << ']' << std::endl;
            }
            break;
            case NLMSG_DONE:
                loop = false;
                break;
            }
    }

    std::cout << std::endl;
    request.header.nlmsg_type = RTM_GETADDR;
    ++request.header.nlmsg_seq;
    if (sendto(fd, &request, sizeof(request), 0, (const sockaddr *)&kernel, request.header.nlmsg_len) < 0)
    {
        std::cerr << "send failed: " << strerror(errno) << std::endl;
        exit(1);
    }

    loop = true;
    char address[49];
    while (loop)
    {
        auto pack_size = read(fd, buf, sizeof(buf));
        if (pack_size < 0)
        {
            std::cerr << "Failed to read netlink: " << strerror(errno) << std::endl;
            continue;
        }

        for (auto h = reinterpret_cast<const nlmsghdr *>(buf); NLMSG_OK(h, pack_size); h = NLMSG_NEXT(h, pack_size))
            switch (h->nlmsg_type)
            {
            case RTM_NEWADDR:
            {
                ifaddrmsg *ifa = (ifaddrmsg *)NLMSG_DATA(h);
                const rtattr *attributes[IFLA_MAX + 1]{};
                take_attributes(attributes, IFA_RTA(ifa), h->nlmsg_len - ((uint8_t *)IFA_RTA(ifa) - (uint8_t *)h));

                if (attributes[IFA_LABEL])
                {
                    auto name = (const char *)RTA_DATA(attributes[IFA_LABEL]);
                    inet_ntop(ifa->ifa_family, RTA_DATA(attributes[IFA_LOCAL]), address, sizeof(address));
                    std::cout << '[' << name << '(' << ifa->ifa_index << ")] [" << address << '/' << +ifa->ifa_prefixlen << ']' << std::endl;
                }
            }
            break;
            case NLMSG_DONE:
                loop = false;
                break;
            }
    }

    return 0;
}
