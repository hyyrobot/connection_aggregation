#include "net_devices_t.h"

#include <sstream>
#include <netpacket/packet.h>

namespace autolabor::connection_aggregation
{
    in_addr net_devices_t::tun_address() const
    {
        return {_tun_address};
    }

    std::pair<actual_msg_t, uint8_t> net_devices_t::receive_from(uint8_t *buffer, size_t size)
    {
        sockaddr_ll remote;
        ip header{};
        struct
        {
            char host[13];
            uint8_t protocol;
            uint16_t id;
        } extra;
        iovec iov[]{
            {.iov_base = &header, .iov_len = sizeof(header)},
            {.iov_base = &extra, .iov_len = sizeof(extra)},
            {.iov_base = buffer, .iov_len = size},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };
        if (recvmsg(_receiver, &msg, 0) <= 0 || header.ip_p != 3)
            return {};
        auto [_, success] = _remotes1.try_emplace(header.ip_src.s_addr, extra.host);
        if (success)
        {
            auto p = _remotes2.find(extra.host);
            if (p == _remotes2.end())
                _remotes2[extra.host] = {header.ip_src.s_addr};
            else
                p->second.insert(header.ip_src.s_addr);
        }
        return {{
                    .remote = header.ip_src,
                    .protocol = extra.protocol,
                    .id = extra.id,

                    .buffer = buffer,
                    .size = static_cast<size_t>(ntohs(header.ip_len) - header.ip_hl * 4),
                },
                remote.sll_ifindex};
    }

    std::unordered_map<unsigned, size_t> net_devices_t::send_to(actual_msg_t msg) const
    {
        std::unordered_map<unsigned, size_t> result;
        auto p = _remotes1.find(msg.remote.s_addr);
        if (p == _remotes1.end())
            for (const auto &device : _devices)
                result[device.first] = device.second.send_to(msg.buffer, msg.size, msg.remote, msg.id);
        else
            for (const auto &device : _devices)
                for (auto remote : _remotes2.at(p->second))
                    result[device.first] = device.second.send_to(msg.buffer, msg.size, in_addr{remote}, msg.id);

        return result;
    }

    std::string net_devices_t::operator[](unsigned index) const
    {
        auto p = _devices.find(index);
        if (p == _devices.end())
            return "unknown";

        std::stringstream builder;
        builder << p->second.name() << '[' << index << ']';
        return builder.str();
    }
} // namespace autolabor::connection_aggregation
