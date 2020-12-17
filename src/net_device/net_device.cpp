#include "net_device.h"

#include <unistd.h> // getpid
#include <arpa/inet.h>

#include <sstream>
#include <cstring>

namespace autolabor::connection_aggregation
{
    std::unordered_map<unsigned, net_device_t> list_devices(const fd_guard_t &fd)
    {
        sockaddr_nl kernel{
            .nl_family = AF_NETLINK,
            .nl_pad = 0,
            .nl_pid = 0,
            .nl_groups = 0,
        };
        struct
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
        if (sendto(fd, &request, sizeof(request), 0, (const sockaddr *)&kernel, request.header.nlmsg_len) < 0)
        {
            std::stringstream builder;
            builder << "Failed to ask addresses, because: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }

        std::unordered_map<unsigned, net_device_t> result;

        auto loop = true;
        uint8_t buffer[8192];
        while (loop)
        {
            auto pack_size = read(fd, buffer, sizeof(buffer));
            if (pack_size < 0)
            {
                std::stringstream builder;
                builder << "Failed to read netlink, because: " << strerror(errno);
                throw std::runtime_error(builder.str());
            }

            for (auto h = reinterpret_cast<const nlmsghdr *>(buffer); NLMSG_OK(h, pack_size); h = NLMSG_NEXT(h, pack_size))
                switch (h->nlmsg_type)
                {
                case RTM_NEWADDR:
                {
                    const char *name = nullptr;
                    in_addr_t address;

                    ifaddrmsg *ifa = (ifaddrmsg *)NLMSG_DATA(h);
                    const rtattr *attributes[IFLA_MAX + 1]{};
                    const rtattr *rta = IFA_RTA(ifa);
                    auto l = h->nlmsg_len - ((uint8_t *)rta - (uint8_t *)h);
                    for (; RTA_OK(rta, l); rta = RTA_NEXT(rta, l))
                        switch (rta->rta_type)
                        {
                        case IFA_LABEL:
                            name = reinterpret_cast<char *>(RTA_DATA(rta));
                            break;
                        case IFA_LOCAL:
                            address = *reinterpret_cast<in_addr_t *>(RTA_DATA(rta));
                            break;
                        }
                    if (name)
                    {
                        auto temp = result.try_emplace(ifa->ifa_index, net_device_t{});
                        if (temp.second)
                            std::strcpy(temp.first->second.name, name);
                        temp.first->second.addresses[address] = ifa->ifa_prefixlen;
                    }
                }
                break;
                case NLMSG_DONE:
                    loop = false;
                    break;
                }
        }

        return result;
    }

    std::ostream &operator<<(std::ostream &stream, const std::unordered_map<unsigned, net_device_t> &devices)
    {
        char text[16];
        for (const auto &device : devices)
        {
            stream << device.second.name << '(' << device.first << "):" << std::endl;
            for (const auto &address : device.second.addresses)
            {
                inet_ntop(AF_INET, &address.first, text, sizeof(text));
                stream << "  " << text << '/' << +address.second << std::endl;
            }
        }
        return stream;
    }
} // namespace autolabor::connection_aggregation