#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <unistd.h>

#include <iostream>

namespace autolabor::connection_aggregation
{
    bool host_t::bind(device_index_t index, uint16_t port)
    {
        fd_guard_t temp(socket(AF_UNIX, SOCK_DGRAM, 0));
        cmd_bind_t msg{
            .type = BIND,
            .index = index,
            .port = port,
        };
        sendto(temp, &msg, sizeof(msg), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));

        return true;
    }

    void host_t::print()
    {
        constexpr static uint8_t msg = VIEW;
        fd_guard_t temp(socket(AF_UNIX, SOCK_DGRAM, 0));
        sendto(temp, &msg, sizeof(msg), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));
    }

    void host_t::device_added(device_index_t index, const char *name)
    {
        if (_devices.try_emplace(index, name, _epoll.operator int(), index).second)
            for (auto &[_, r] : _remotes)
                r.device_added(index);
    }

    void host_t::device_removed(device_index_t index)
    {
        if (_devices.erase(index))
            for (auto &[_, r] : _remotes)
                r.device_added(index);
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
                const auto ifi = reinterpret_cast<ifinfomsg *>(NLMSG_DATA(h));
                const auto index = ifi->ifi_index;
                if (ifi->ifi_flags & IFF_UP)
                {
                    const rtattr *rta = IFLA_RTA(ifi);
                    for (auto l = h->nlmsg_len - ((uint8_t *)rta - (uint8_t *)h);
                         RTA_OK(rta, l);
                         rta = RTA_NEXT(rta, l))
                        if (rta->rta_type == IFLA_IFNAME)
                        {
                            auto name = reinterpret_cast<char *>(RTA_DATA(rta));
                            if (name && std::strcmp(name, _name) != 0 && std::strcmp(name, "lo") != 0)
                                device_added(index, name);
                            break;
                        }
                }
                else // 关闭的网卡相当于删除
                    device_removed(index);
                break;
            }

            case RTM_DELLINK:
                device_removed(reinterpret_cast<ifinfomsg *>(NLMSG_DATA(h))->ifi_index);
                break;
            }
    }

    void host_t::read_unix(uint8_t *buffer, size_t size)
    {
        auto n = read(_unix, buffer, size);
        switch (*buffer)
        {
        case VIEW:
            std::cout << to_string() << std::endl;
            break;
        case BIND:
            if (n == sizeof(cmd_bind_t))
            {
                auto msg = reinterpret_cast<cmd_bind_t *>(buffer);
                auto p = _devices.find(msg->index);

                if (p == _devices.end())
                    std::cerr << "devices[" << msg->index << "] not exist" << std::endl;
                else
                    try
                    {
                        p->second.bind(msg->port);
                        std::cout << "bind devices[" << msg->index << "] to port " << msg->port << std::endl;
                    }
                    catch (std::runtime_error &e)
                    {
                        std::cerr << e.what() << std::endl;
                    }
            }
            break;
        case YELL:
            send_void({}, false);
            break;
        case SEND_HANDSHAKE:
            if (n == 5)
                send_void(*reinterpret_cast<in_addr *>(buffer + 1), false);
            break;
        case ADD_REMOTE:
            if (n == sizeof(cmd_remote_t))
            {
                auto msg = reinterpret_cast<cmd_remote_t *>(buffer);
                add_remote_inner(msg->virtual_, msg->actual_, msg->port);
            }
            break;
        }
    }

    std::string host_t::to_string() const
    {
        std::stringstream builder;
        char text[16];
        inet_ntop(AF_INET, &_address, text, sizeof(text));
        builder << _name << '(' << _index << "): " << text << " | ";

        if (_devices.empty())
            builder << "devices: []";
        else
        {
            auto p = _devices.begin();
            builder << "devices: [" << p->first;
            for (++p; p != _devices.end(); ++p)
                builder << ", " << p->first;
            builder << ']';
        }

        builder << " | threads = " << _threads.size() << std::endl;

        if (_remotes.empty())
            return builder.str();

        builder << std::endl
                << std::endl
                << remote_t::TITLE << std::endl
                << remote_t::_____ << std::endl;

        connection_key_union _union;
        connection_t::snapshot_t snapshot;
        for (auto &[a, r] : _remotes)
        {
            auto buffer = r.to_string();
            inet_ntop(AF_INET, &a, buffer.data() + 2, 16);
            for (auto &c : buffer)
                if (!c)
                    c = ' ';
            builder << buffer;
        }
        return builder.str();
    }

} // namespace autolabor::connection_aggregation
