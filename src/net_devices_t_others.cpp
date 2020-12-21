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
        // 记录对方的名字、地址
        if (recvmsg(_receiver, &msg, 0) <= 0 || header.ip_p != 3)
            return {};
        actual_msg_t result{
            .remote = header.ip_src,
            .protocol = extra.protocol,
            .id = extra.id,

            .buffer = buffer,
            .size = static_cast<size_t>(ntohs(header.ip_len) - header.ip_hl * 4),
        };
        return {result, remote.sll_ifindex};
    }

    std::unordered_map<unsigned, size_t> net_devices_t::send_to(actual_msg_t msg) const
    {
        std::unordered_map<unsigned, size_t> result;
        // if(查到对应主机)
        //   遍历去往目标主机的连接并发送
        // else
        //   从每个端口发送
        for (const auto &device : _devices)
            result[device.first] = device.second.send_to(msg.buffer, msg.size, msg.remote, msg.id);
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
