#include "../program_t.h"

#include <unistd.h>

#include <sstream>
#include <cstring>

namespace autolabor::connection_aggregation
{
    void program_t::local_monitor()
    {
        uint8_t buffer[4096];
        while (true)
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
                case RTM_NEWLINK:
                {
                    const ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(h);
                    if (ifi->ifi_flags & IFF_UP)
                    {
                        const rtattr *rta = IFLA_RTA(ifi);
                        auto l = h->nlmsg_len - ((uint8_t *)rta - (uint8_t *)h);
                        for (; RTA_OK(rta, l); rta = RTA_NEXT(rta, l))
                            if (rta->rta_type == IFLA_IFNAME)
                            {
                                const char *name = reinterpret_cast<char *>(RTA_DATA(rta));
                                if (name && std::strcmp(name, _tun.name()) != 0 && std::strcmp(name, "lo") != 0)
                                    address_added(ifi->ifi_index, name);
                                break;
                            }
                    }
                    else // 关闭的网卡相当于删除
                        device_removed(((struct ifinfomsg *)NLMSG_DATA(h))->ifi_index);
                    break;
                }

                case RTM_NEWADDR:
                {
                    const char *name = nullptr;
                    in_addr address;

                    const ifaddrmsg *ifa = (ifaddrmsg *)NLMSG_DATA(h);
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
                    if (name && std::strcmp(name, _tun.name()) != 0 && std::strcmp(name, "lo") != 0)
                        address_added(ifa->ifa_index, name, address);
                    break;
                }

                case RTM_DELLINK:
                    device_removed(((struct ifinfomsg *)NLMSG_DATA(h))->ifi_index);
                    break;

                case RTM_DELADDR:
                {
                    in_addr address;

                    const ifaddrmsg *ifa = (ifaddrmsg *)NLMSG_DATA(h);
                    const rtattr *rta = IFA_RTA(ifa);
                    auto l = h->nlmsg_len - ((uint8_t *)rta - (uint8_t *)h);
                    for (; RTA_OK(rta, l); rta = RTA_NEXT(rta, l))
                        if (rta->rta_type == IFA_LOCAL)
                        {
                            address_removed(ifa->ifa_index, *reinterpret_cast<in_addr *>(RTA_DATA(rta)));
                            break;
                        }
                    break;
                }
                }
        }
    }

    void program_t::address_added(unsigned index, const char *name, in_addr address)
    {
        { // 修改本机网卡表
            WRITE_GRAUD(_local_mutex);
            auto [p, new_device] = _devices.try_emplace(index, name);
            p->second.push_address(address);
            if (!new_device) // 如果没有增加新的网卡，退出
                return;
        }
        // 修改连接表
        READ_GRAUD(_remote_mutex);
        WRITE_GRAUD(_connection_mutex);
        connection_key_union key{.src_index = index};
        for (const auto &[address, remote] : _remotes)
        {
            auto &p = _connections[address];
            for (const auto &[remote_index, _] : remote)
            {
                key.dst_index = remote_index;
                p.emplace(std::piecewise_construct,
                          std::forward_as_tuple(key.key),
                          std::forward_as_tuple());
            }
        }
    }

    void program_t::address_removed(unsigned index, in_addr address)
    {
        // 修改本机网卡表
        // 即使没有 ip 地址的网卡也可以用于发送
        WRITE_GRAUD(_local_mutex);
        auto p = _devices.find(index);
        if (p != _devices.end())
            p->second.erase_address(address);
    }

    void program_t::device_removed(unsigned index)
    {
        {
            WRITE_GRAUD(_local_mutex);
            auto p = _devices.find(index);
            if (p != _devices.end())
                _devices.erase(p);
            else
                return; // 试图移除不存在的网卡
        }
        // 修改连接表
        READ_GRAUD(_remote_mutex);
        WRITE_GRAUD(_connection_mutex);
        connection_key_union key{.src_index = index};
        for (const auto &[address, remote] : _remotes)
        {
            auto &p = _connections[address];
            for (const auto &[remote_index, _] : remote)
            {
                key.dst_index = remote_index;
                p.erase(key.key);
            }
        }
    }

} // namespace autolabor::connection_aggregation