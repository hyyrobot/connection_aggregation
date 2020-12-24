#include "../program_t.h"

#include <arpa/inet.h>

namespace autolabor::connection_aggregation
{
    const char *program_t::name() const
    {
        return _tun.name();
    }

    in_addr program_t::address() const
    {
        return _tun.address();
    }

    std::ostream &operator<<(std::ostream &out, const program_t &program)
    {
        // tun
        out << "- tun device: " << program._tun.name() << '(' << program._tun.index() << ')' << std::endl;
        char text[IFNAMSIZ];
        // local
        out << "- local devices:" << std::endl;
        {
            std::shared_lock<std::shared_mutex> lock(program._local_mutex);
            for (const auto &[i, d] : program._devices)
            {
                auto address = d.address();
                inet_ntop(AF_INET, &address, text, sizeof(text));
                out << "  - " << d.name() << '(' << i << "): " << text << std::endl;
            }
        }

        // remote
        out << "- remote devices:" << std::endl;
        {
            std::shared_lock<std::shared_mutex> lock(program._remote_mutex);
            for (const auto &[a, d] : program._remotes)
            {
                inet_ntop(AF_INET, &a, text, sizeof(text));
                out << "  - " << text << ':' << std::endl;
                for (const auto &[i, b] : d)
                {
                    inet_ntop(AF_INET, &b, text, sizeof(text));
                    out << "    - " << i << ": " << text << std::endl;
                }
            }
        }

        // connection
        out << "- connections:" << std::endl;
        {
            std::shared_lock<std::shared_mutex> lock(program._connection_mutex);
            for (const auto &[a, d] : program._connections)
            {
                inet_ntop(AF_INET, &a, text, sizeof(text));
                out << "  - " << text << ':' << std::endl;
                for (const auto &[key, info] : d)
                {
                    connection_key_union x{.key = key};
                    out << "    - " << x.src_index << " -> " << x.dst_index << " : " << info.next_id() << std::endl;
                }
            }
        }

        return out;
    }
} // namespace autolabor::connection_aggregation