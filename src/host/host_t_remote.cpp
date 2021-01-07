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

    void host_t::add_remote_inner(in_addr virtual_, in_addr actual_, uint16_t port)
    {
        auto &r = _remotes.try_emplace(virtual_.s_addr).first->second;
        std::vector<device_index_t> indices(_devices.size());
        auto p = _devices.begin();
        for (auto q = indices.begin(); q != indices.end(); ++p, ++q)
            *q = p->first;
        r.add_direct(actual_, port, indices);
    }

    void host_t::add_route_inner(in_addr dst, in_addr next, uint8_t distance)
    {
        _remotes.try_emplace(dst.s_addr).first->second.add_route(next, distance);
    }

} // namespace autolabor::connection_aggregation