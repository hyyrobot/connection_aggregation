#include "net_devices_t.h"

#include <sstream>

namespace autolabor::connection_aggregation
{
    size_t net_devices_t::receive(msghdr *msg) const
    {
        // 记录对方的名字、地址
        return recvmsg(_receiver, msg, 0);
    }

    std::unordered_map<unsigned, size_t> net_devices_t::send(msghdr *msg) const
    {
        std::unordered_map<unsigned, size_t> result;
        // if(查到对应主机)
        //   遍历去往目标主机的连接并发送
        // else
        //   从每个端口发送
        for (const auto &device : _devices)
            result[device.first] = sendmsg(device.second.out, msg, 0);
        return result;
    }

    std::string net_devices_t::operator[](unsigned index) const
    {
        auto p = _devices.find(index);
        if (p == _devices.end())
            return "unknown";

        std::stringstream builder;
        builder << p->second.name << '[' << index << ']';
        return builder.str();
    }
} // namespace autolabor::connection_aggregation
