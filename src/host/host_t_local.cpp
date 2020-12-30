#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <unistd.h>
#include <linux/rtnetlink.h>

namespace autolabor::connection_aggregation
{
    void host_t::bind(device_index_t index, uint16_t port)
    {
        READ_GRAUD(_device_mutex);
        _devices.at(index).bind(port);
    }

    void host_t::device_added(device_index_t index, const char *name)
    {
        READ_GRAUD(_device_mutex);
        _devices.try_emplace(index, name);
    }

    void host_t::device_removed(device_index_t index)
    {
        READ_GRAUD(_device_mutex);
        auto p = _devices.find(index);
        if (p == _devices.end())
            return;
        _devices.erase(index);
    }

    void host_t::local_monitor()
    {
        uint8_t buffer[4096];
        while (true)
        {
            auto pack_size = read(_netlink, buffer, sizeof(buffer));
            if (pack_size < 0)
                THROW_ERRNO(__FILE__, __LINE__, "read netlink");

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
                                if (name && std::strcmp(name, _name) != 0 && std::strcmp(name, "lo") != 0)
                                    device_added(ifi->ifi_index, name);
                                break;
                            }
                    }
                    else // 关闭的网卡相当于删除
                        device_removed(((struct ifinfomsg *)NLMSG_DATA(h))->ifi_index);
                    break;
                }

                case RTM_DELLINK:
                    device_removed(((struct ifinfomsg *)NLMSG_DATA(h))->ifi_index);
                    break;
                }
        }
    }

} // namespace autolabor::connection_aggregation