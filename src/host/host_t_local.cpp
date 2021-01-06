#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <linux/rtnetlink.h>
#include <unistd.h>

namespace autolabor::connection_aggregation
{
    bool host_t::bind(device_index_t index, uint16_t port)
    {
        READ_LOCK(_device_mutex);
        auto p = _devices.find(index);

        if (p == _devices.end())
            return false;

        p->second.bind(port);
        return true;
    }

    void host_t::device_added(device_index_t index, const char *name)
    {
        {
            READ_LOCK(_device_mutex);
            if (!_devices.try_emplace(index, name, _epoll.operator int(), index).second)
                return;
        }
        connection_key_union _union{.pair{.src_index = index}};
        {
            READ_LOCK(_srand_mutex);
            for (auto &[_, s] : _srands)
            {
                read_lock lp(s.port_mutex);
                write_lock lc(s.connection_mutex);
                for (const auto [port, _] : s.ports)
                {
                    _union.pair.dst_port = port;
                    s.connections.try_emplace(_union.key);
                }
            }
        }
    }

    void host_t::device_removed(device_index_t index)
    {
        {
            READ_LOCK(_device_mutex);
            if (!_devices.erase(index))
                return;
        }
        connection_key_union _union{.pair{.src_index = index}};
        {
            READ_LOCK(_srand_mutex);
            for (auto &[_, s] : _srands)
            {
                read_lock lp(s.port_mutex);
                write_lock lc(s.connection_mutex);
                for (const auto [port, _] : s.ports)
                {
                    _union.pair.dst_port = port;
                    s.connections.erase(_union.key);
                }
            }
        }
    }

    void host_t::read_netlink(uint8_t *buffer, size_t size)
    {
        auto n = read(_netlink, buffer, size);
        if (n < 0)
            THROW_ERRNO(__FILE__, __LINE__, "read netlink");

        for (auto h = reinterpret_cast<const nlmsghdr *>(buffer); NLMSG_OK(h, n); h = NLMSG_NEXT(h, n))
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

} // namespace autolabor::connection_aggregation