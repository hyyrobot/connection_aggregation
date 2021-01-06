#include "../host_t.h"

#include <netinet/ip.h>

namespace autolabor::connection_aggregation
{
    void host_t::add_remote(in_addr virtual_, in_addr actual_, uint16_t port)
    {
        fd_guard_t temp = socket(AF_UNIX, SOCK_DGRAM, 0);
        msg_remote_t msg{
            .type = ADD_REMOTE,
            .port = port,
            .virtual_ = virtual_,
            .actual_ = actual_,
        };
        sendto(temp, &msg, sizeof(msg_remote_t), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));
    }

    void host_t::add_route(in_addr dst, in_addr next, uint8_t distance)
    {
        add_route_inner(dst, next, distance);
    }

    void host_t::add_remote_inner(in_addr remote_host, uint16_t port, in_addr address)
    {
        // 增加远程对象
        auto [p, b] = _srands.try_emplace(remote_host.s_addr);
        // 增加连接
        auto &s = p->second;
        {
            write_lock lp(s.port_mutex);
            auto [q, b] = s.ports.try_emplace(port, address);
            if (b)
            {
                READ_LOCK(_device_mutex);
                write_lock lc(s.connection_mutex);
                connection_key_union _union{.pair{.dst_port = port}};
                for (const auto &[i, _] : _devices)
                {
                    _union.pair.src_index = i;
                    s.connections.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(_union.key),
                        std::forward_as_tuple());
                }
            }
            else
                q->second = address;
        }
        { // 更新路由表
            WRITE_LOCK(_route_mutex);
            auto [p, b] = _route.try_emplace(remote_host.s_addr);
            p->second = {}; // 空的表示直连路由
        }
    }

    void host_t::add_route_inner(in_addr dst, in_addr next, uint8_t distance)
    {
        if (!distance)
            throw std::runtime_error("to add direct route, use add remote instead");
        if (_srands.find(next.s_addr) == _srands.end())
            throw std::runtime_error("invalid next step address");
        {
            WRITE_LOCK(_route_mutex);
            auto [p, b] = _route.try_emplace(dst.s_addr);
            // 没见过的目的地址，或旧的路由不如新的好
            if (b || p->second.distance >= distance)
                p->second = {.next = next, .distance = distance};
        }
    }

} // namespace autolabor::connection_aggregation