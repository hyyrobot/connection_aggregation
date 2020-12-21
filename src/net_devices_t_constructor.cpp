#include "net_devices_t.h"

#include <fcntl.h>     // open
#include <sys/ioctl.h> // ioctl
#include <unistd.h>    // getpid
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>

#include <sstream>
#include <cstring>
#include <iostream>

namespace autolabor::connection_aggregation
{
    net_devices_t::net_devices_t(const char *host)
        : _tun(open("/dev/net/tun", O_RDWR)),
          _netlink(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)),
          _receiver(socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP)))
    {
        // 顺序不能变：
        // 1. 绑定 netlink
        void bind_netlink(const fd_guard_t &, uint32_t);
        // 2. 注册 TUN
        void register_tun(const fd_guard_t &, char *);
        // 3. 获取 TUN index
        uint32_t wait_tun_index(const fd_guard_t &, const char *);
        // 4. 配置 TUN
        void config_tun(const fd_guard_t &, uint32_t);

        bind_netlink(_netlink, RTMGRP_LINK | RTMGRP_IPV4_IFADDR);
        register_tun(_tun, _tun_name);
        config_tun(_netlink, _tun_index = wait_tun_index(_netlink, _tun_name));

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
        if (sendto(_netlink, &request, sizeof(request), 0, (const sockaddr *)&kernel, request.header.nlmsg_len) < 0)
        {
            std::stringstream builder;
            builder << "Failed to ask addresses, because: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }

        auto loop = true;
        uint8_t buffer[8192];
        while (loop)
        {
            auto pack_size = read(_netlink, buffer, sizeof(buffer));
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
                    in_addr address;

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
                            address = *reinterpret_cast<in_addr *>(RTA_DATA(rta));
                            break;
                        }
                    if (name && std::strcmp(name, _tun_name) != 0 && std::strcmp(name, "lo") != 0)
                    {
                        auto [p, success] = _devices.try_emplace(ifa->ifa_index, host, name);
                        if (success)
                            p->second.push_address(address);
                    }
                }
                break;
                case NLMSG_DONE:
                    loop = false;
                    break;
                }
        }

        { // 显示内部细节
            std::stringstream builder;
            builder << _tun_name << '(' << _tun_index << ')' << std::endl;
            char text[16];
            for (const auto &device : _devices)
            {
                auto address = device.second.address();
                inet_ntop(AF_INET, &address, text, sizeof(text));
                builder << device.second.name() << '(' << device.first << "): " << text << std::endl;
            }
            std::cout << builder.str() << std::endl;
        }
    }

    void bind_netlink(const fd_guard_t &netlink, uint32_t group)
    {
        const static sockaddr_nl local{
            .nl_family = AF_NETLINK,
            .nl_pad = 0,
            .nl_pid = static_cast<unsigned>(getpid()),
            .nl_groups = group,
        };
        if (bind(netlink, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) != 0)
        {
            std::stringstream builder;
            builder << "Failed to bind netlink socket, because: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }
    }

    void register_tun(const fd_guard_t &fd, char *name)
    {
        ifreq request{.ifr_ifru{.ifru_flags = IFF_TUN | IFF_NO_PI}};
        if (ioctl(fd, TUNSETIFF, (void *)&request) < 0)
        {
            std::stringstream builder;
            builder << "Failed to register tun, because: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }
        std::strcpy(name, request.ifr_name);
    }

    uint32_t wait_tun_index(const fd_guard_t &fd, const char *name)
    {
        uint8_t buf[2048];

        while (true)
        {
            auto pack_size = read(fd, buf, sizeof(buf));
            if (pack_size < 0)
            {
                std::stringstream builder;
                builder << "Failed to read netlink, because: " << strerror(errno);
                throw std::runtime_error(builder.str());
            }

            for (auto h = reinterpret_cast<const nlmsghdr *>(buf); NLMSG_OK(h, pack_size); h = NLMSG_NEXT(h, pack_size))
                if (h->nlmsg_type == RTM_NEWLINK)
                {
                    ifinfomsg *ifi = (ifinfomsg *)NLMSG_DATA(h);
                    const rtattr *rta = IFLA_RTA(ifi);
                    auto l = h->nlmsg_len - ((uint8_t *)rta - (uint8_t *)h);
                    for (; RTA_OK(rta, l); rta = RTA_NEXT(rta, l))
                        if (rta->rta_type == IFLA_IFNAME && std::strcmp((char *)RTA_DATA(rta), name) == 0)
                            return ifi->ifi_index;
                }
        }
    }

    void config_tun(const fd_guard_t &fd, uint32_t index)
    {
        // 设置网卡到启动状态的请求包
        struct
        {
            nlmsghdr header;
            ifinfomsg message;
        } request_link{
            .header{
                .nlmsg_len = sizeof(request_link),
                .nlmsg_type = RTM_NEWLINK,
                .nlmsg_flags = NLM_F_REQUEST, // 这是一个改变设备状态的请求
                .nlmsg_seq = 0,
                .nlmsg_pid = 0,
            },
            .message{
                .ifi_family = AF_UNSPEC,
                .ifi_type = 0,
                .ifi_index = static_cast<int>(index),
                .ifi_flags = IFF_UP | IFF_NOARP | IFF_MULTICAST | IFF_BROADCAST,
                .ifi_change = 0xffffffff, // 这是一个无用的常量，必修填全 1
            }};

        // 为网卡添加 ip 地址的请求包
        struct
        {
            nlmsghdr header;
            ifaddrmsg message;
            rtattr attributes;
            uint8_t data[4];
        } request_address{
            .header{
                .nlmsg_len = sizeof(request_address),
                .nlmsg_type = RTM_NEWADDR,
                .nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL, // 这是一个创建新地址的请求
                .nlmsg_seq = 1,
                .nlmsg_pid = 0,
            },
            .message{
                .ifa_family = AF_INET, // 协议簇
                .ifa_prefixlen = 24,   // 子网长度
                .ifa_flags = 0,
                .ifa_scope = 0,
                .ifa_index = static_cast<unsigned>(index),
            },
            .attributes{
                .rta_len = sizeof(rtattr) + sizeof(request_address.data),
                .rta_type = IFA_LOCAL,
            },
            .data{10, 10, 10, 10},
        };

        iovec iov[]{
            {.iov_base = &request_link, .iov_len = request_link.header.nlmsg_len},
            {.iov_base = &request_address, .iov_len = request_address.header.nlmsg_len},
        };
        sockaddr_nl kernel{
            .nl_family = AF_NETLINK,
            .nl_pid = 0,
            .nl_groups = 0,
        };
        msghdr msg{
            .msg_name = &kernel,
            .msg_namelen = sizeof(kernel),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };

        if (sendmsg(fd, &msg, 0) < 0)
        {
            std::stringstream builder;
            builder << "Failed to set network ip, because: " << strerror(errno);
            throw std::runtime_error(builder.str());
        }
    }

} // namespace autolabor::connection_aggregation
