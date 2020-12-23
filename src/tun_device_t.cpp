#include "tun_device_t.h"

#include <fcntl.h>     // open
#include <sys/ioctl.h> // ioctl
#include <unistd.h>    // read
#include <linux/if_tun.h>

#include <sstream>
#include <cstring>

namespace autolabor::connection_aggregation
{
    tun_device_t::tun_device_t(const fd_guard_t &netlink, const char *name, in_addr address)
        : _tun(open("/dev/net/tun", O_RDWR)),
          _address(address)
    {
        // 顺序不能变：
        // 1. 注册 TUN
        void register_tun(const fd_guard_t &, char *);
        // 2. 获取 TUN index
        uint32_t wait_tun_index(const fd_guard_t &, const char *);
        // 3. 配置 TUN
        void config_tun(const fd_guard_t &, uint32_t, in_addr);

        std::strcpy(_name, name);
        register_tun(_tun, _name);
        config_tun(netlink, _index = wait_tun_index(netlink, _name), _address);
    }

    const char *tun_device_t::name() const
    {
        return _name;
    }
    unsigned tun_device_t::index() const
    {
        return _index;
    }

    void register_tun(const fd_guard_t &fd, char *name)
    {
        ifreq request{.ifr_ifru{.ifru_flags = IFF_TUN | IFF_NO_PI}};
        std::strcpy(request.ifr_name, name);
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
            in_addr address;
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
                .rta_len = sizeof(rtattr) + sizeof(request_address.address),
                .rta_type = IFA_LOCAL,
            },
            .address = address,
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
