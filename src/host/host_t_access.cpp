#include "../host_t.h"

#include <arpa/inet.h>

#include <cstring>

namespace autolabor::connection_aggregation
{
    std::ostream &operator<<(std::ostream &o, const host_t &host)
    {
        char text[16];
        inet_ntop(AF_INET, &host._address, text, sizeof(text));
        o << host._name << '(' << host._index << "): " << text << " | ";

        if (host._devices.empty())
            o << "devices: []" << std::endl;
        else
        {
            read_lock l(host._device_mutex);
            auto p = host._devices.begin();
            o << "devices: [" << p->first;
            for (++p; p != host._devices.end(); ++p)
                o << ", " << p->first;
            o << ']';
        }

        if (host._srands.empty())
            return o;

        const static std::string
            title = "|      host       | index |  port  |     address     | state | input | output |",
            _____ = "| --------------- | ----- | ------ | --------------- | ----- | ----- | ------ |",
            space = "|                 |       |        |                 |  -->  |       |        |";
        o << std::endl
          << std::endl
          << title << std::endl
          << _____ << std::endl;

        connection_key_union _union;
        connection_t::snapshot_t snapshot;
        {
            read_lock l(host._srand_mutex);

            for (auto &[a, s] : host._srands)
            {
                inet_ntop(AF_INET, &a, text, sizeof(text));
                read_lock ll(s.connection_mutex);
                for (const auto &[k, c] : s.connections)
                {
                    auto buffer = space;
                    auto b = buffer.data();
                    std::strcpy(b + 2, text);
                    _union.key = k;
                    std::to_string(_union.pair.src_index).copy(b + 22, 5);
                    std::to_string(_union.pair.dst_port).copy(b + 29, 6);
                    {
                        read_lock lll(s.port_mutex);
                        auto a = s.ports.at(_union.pair.dst_port);
                        inet_ntop(AF_INET, &a, b + 37, 17);
                    }
                    c.snapshot(&snapshot);
                    b[55] = snapshot.state + '0';
                    b[59] = snapshot.opposite + '0';
                    std::to_string(snapshot.received).copy(b + 63, 5);
                    std::to_string(snapshot.sent).copy(b + 71, 6);
                    for (auto &c : buffer)
                        if (c < ' ')
                            c = ' ';
                    o << buffer << std::endl;
                }
            }
        }

        return o;
    }
} // namespace autolabor::connection_aggregation