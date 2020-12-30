#include "srand_t.h"

#include <arpa/inet.h>

namespace autolabor::connection_aggregation
{
    std::ostream &operator<<(std::ostream &o, const srand_t &srand)
    {
        std::shared_lock<std::shared_mutex>(srand._port_mutex);
        if (srand._ports.empty())
            return o << "[]";

        char text[16];
        auto p = srand._ports.begin();
        inet_ntop(AF_INET, &p->second, text, sizeof(text));
        o << '[' << text << ':' << p->first;
        for (++p; p != srand._ports.end(); ++p)
        {
            inet_ntop(AF_INET, &p->second, text, sizeof(text));
            o << " ," << text << ':' << p->first;
        }
        return o << ']';
    }

    void srand_t::device_detected(device_index_t index)
    {
        connection_key_union _union{.src_index = index};
        {
            READ_GRAUD(_port_mutex);
            WRITE_GRAUD(_connection_mutex);
            for (auto [port, _] : _ports)
            {
                _union.dst_port = port;
                _connections.try_emplace(_union.key);
            }
        }
    }

    void srand_t::device_removed(device_index_t index)
    {
        connection_key_union _union{.src_index = index};
        {
            READ_GRAUD(_port_mutex);
            WRITE_GRAUD(_connection_mutex);
            for (auto [port, _] : _ports)
            {
                _union.dst_port = port;
                _connections.erase(_union.key);
            }
        }
    }

    bool srand_t::remote_detected(uint16_t port, in_addr address)
    {
        WRITE_GRAUD(_port_mutex);
        auto [p, b] = _ports.try_emplace(port, address);
        if (!b)
            p->second = address;
        return b;
    }

} // namespace autolabor::connection_aggregation