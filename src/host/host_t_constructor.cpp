#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <fcntl.h>     // open
#include <sys/ioctl.h> // ioctl
#include <unistd.h>    // read
#include <sys/epoll.h> // epoll
#include <linux/rtnetlink.h>
#include <linux/if_tun.h>

namespace autolabor::connection_aggregation
{
    host_t::host_t(const char *name, in_addr address)
        : _address(address),
          _netlink(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)),
          _tun(open("/dev/net/tun", O_RDWR)),
          _unix(socket(AF_UNIX, SOCK_DGRAM, 0)),
          _address_un{.sun_family = AF_UNIX},
          _epoll(epoll_create1(0))
    {
        // 绑定 netlink
        sockaddr_nl netlink{
            .nl_family = AF_NETLINK,
            .nl_pid = static_cast<unsigned>(getpid()),
            .nl_groups = RTMGRP_LINK,
        };
        if (::bind(_netlink, reinterpret_cast<sockaddr *>(&netlink), sizeof(netlink)))
            THROW_ERRNO(__FILE__, __LINE__, "bind netlink")
        // 创建 tun
        ifreq request{.ifr_ifru{.ifru_flags = IFF_TUN | IFF_NO_PI}};
        std::strcpy(request.ifr_name, name);
        if (ioctl(_tun, TUNSETIFF, (void *)&request) < 0)
            THROW_ERRNO(__FILE__, __LINE__, "register tun")
        std::strcpy(_name, request.ifr_name);
        // 启动 tun 并配置 ip 地址
        uint32_t wait_tun_index(const fd_guard_t &, const char *);
        void config_tun(const fd_guard_t &, uint32_t, in_addr);
        config_tun(_netlink, _index = wait_tun_index(_netlink, _name), _address);
        // 绑定本机控制接口
        std::strcpy(_address_un.sun_path, _name);
        std::strcpy(_address_un.sun_path + std::strlen(_name), ".interface");
        unlink(_address_un.sun_path);
        if (::bind(_unix, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un)))
            THROW_ERRNO(__FILE__, __LINE__, "bind unix")
        // 注册 tun 到 epoll
        epoll_event event{.events = EPOLLIN, .data{.u32 = ID_TUN}};
        if (epoll_ctl(_epoll, EPOLL_CTL_ADD, _tun, &event))
            THROW_ERRNO(__FILE__, __LINE__, "add tun to epoll");
        // 注册 unix 到 epoll
        event.data.u32 = ID_UNIX;
        if (epoll_ctl(_epoll, EPOLL_CTL_ADD, _unix, &event))
            THROW_ERRNO(__FILE__, __LINE__, "add unix to epoll");
        // 注册 netlink 到 epoll
        event.data.u32 = ID_NETLINK;
        if (epoll_ctl(_epoll, EPOLL_CTL_ADD, _netlink, &event))
            THROW_ERRNO(__FILE__, __LINE__, "add netlink to epoll");
    }

    uint32_t wait_tun_index(const fd_guard_t &fd, const char *name)
    {
        uint8_t buf[2048];

        while (true)
        {
            auto pack_size = read(fd, buf, sizeof(buf));
            if (pack_size < 0)
                THROW_ERRNO(__FILE__, __LINE__, "read netlink")

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

    constexpr static sockaddr_nl KERNEL{.nl_family = AF_NETLINK};
    void config_tun(const fd_guard_t &fd, uint32_t index, in_addr address)
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
            },
            .message{
                .ifi_family = AF_UNSPEC,
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
            in_addr address;
        } request_address{
            .header{
                .nlmsg_len = sizeof(request_address),
                .nlmsg_type = RTM_NEWADDR,
                .nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL, // 这是一个创建新地址的请求
                .nlmsg_seq = 1,
            },
            .message{
                .ifa_family = AF_INET, // 协议簇
                .ifa_prefixlen = 24,   // 子网长度
                .ifa_index = static_cast<unsigned>(index),
            },
            .attributes{
                .rta_len = sizeof(rtattr) + sizeof(request_address.address),
                .rta_type = IFA_LOCAL,
            },
            .address = address,
        };

        iovec iov[]{
            {.iov_base = &request_link, .iov_len = request_link.header.nlmsg_len},
            {.iov_base = &request_address, .iov_len = request_address.header.nlmsg_len},
        };
        msghdr msg{
            .msg_name = (void *)&KERNEL,
            .msg_namelen = sizeof(KERNEL),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };

        if (!sendmsg(fd, &msg, MSG_WAITALL))
            THROW_ERRNO(__FILE__, __LINE__, "set ip to tun")
    }

    void host_t::send_list_request() const
    {
        const struct
        {
            nlmsghdr header;
            rtgenmsg message;
        } request{
            .header{
                .nlmsg_len = sizeof(request),
                .nlmsg_type = RTM_GETLINK,
                .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
                .nlmsg_pid = static_cast<unsigned>(getpid()),
            },
            .message{
                .rtgen_family = AF_UNSPEC,
            },
        };
        if (sendto(_netlink, &request, sizeof(request), MSG_WAITALL,
                   reinterpret_cast<const sockaddr *>(&KERNEL), sizeof(KERNEL)) <= 0)
            THROW_ERRNO(__FILE__, __LINE__, "get links")
    }

} // namespace autolabor::connection_aggregation
