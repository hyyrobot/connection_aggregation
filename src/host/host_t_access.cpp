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
            std::shared_lock<std::shared_mutex>(host._device_mutex);
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
        return o;
    }
} // namespace autolabor::connection_aggregation