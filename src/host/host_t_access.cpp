#include "../host_t.h"

#include <arpa/inet.h>

namespace autolabor::connection_aggregation
{
    std::ostream &operator<<(std::ostream &o, const host_t &host)
    {
        char text[16];
        inet_ntop(AF_INET, &host._address, text, sizeof(text));
        o << host._name << '(' << host._index << "): " << text << std::endl;
        {
            read_lock l(host._device_mutex);
            if (host._devices.empty())
                o << "devices: []" << std::endl;
            else
            {
                auto p = host._devices.begin();
                o << "devices: [" << p->first;
                for (++p; p != host._devices.end(); ++p)
                    o << ", " << p->first;
                o << ']' << std::endl;
            }
        }
        {
            read_lock l(host._srand_mutex);
            if (host._srands.empty())
                o << "srands: []" << std::endl;
            else
            {
                o << "srands:" << std::endl;
                for (auto &[a, s] : host._srands)
                {
                    inet_ntop(AF_INET, &a, text, sizeof(text));
                    o << "  " << text << ": [";
                    read_lock ll(s.port_mutex);
                    auto p = s.ports.begin();
                    inet_ntop(AF_INET, &p->second, text, sizeof(text));
                    o << text << ':' << p->first;
                    for (++p; p != s.ports.end(); ++p)
                    {
                        inet_ntop(AF_INET, &p->second, text, sizeof(text));
                        o << ", " << text << ':' << p->first;
                    }
                    o << ']' << std::endl;
                }
            }
        }

        return o;
    }
} // namespace autolabor::connection_aggregation