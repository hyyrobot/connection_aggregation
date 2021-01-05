#include "host_t.h"
#include "cin_to_list.h"

#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <list>
#include <thread>
#include <iostream>

using namespace autolabor::connection_aggregation;
using namespace std::chrono_literals;

bool script(host_t &h, const std::list<std::string> &commands)
{
    auto p = commands.begin();
    switch (commands.size())
    {
    case 1:
        if (*p == "view")
        {
            std::cout << h << std::endl;
            return true;
        }
        if (*p == "yell")
        {
            std::cout << "sent " << h.send_handshake({}, false) << " packages" << std::endl;
            std::this_thread::sleep_for(200ms);
            std::cout << h << std::endl;
            return true;
        }
        return false;

    case 2:
        if (*p++ == "shakehand")
        {
            in_addr a{};
            inet_pton(AF_INET, p->data(), &a);
            std::cout << "sent " << h.send_handshake(a, false) << " packages" << std::endl;
            std::this_thread::sleep_for(200ms);
            std::cout << h << std::endl;
            return true;
        }
        return false;

    case 3:
        if (*p == "bind")
        {
            auto index = std::stod(*++p);
            auto port = std::stod(*++p);
            (h.bind(index, port)
                 ? std::cout << "bind device[" << index << "] with port " << port
                 : std::cout << "bind failed")
                << std::endl;
            return true;
        }
        return false;

    case 4:
        if (*p == "remote")
        {
            in_addr a0, a1;
            inet_pton(AF_INET, (++p)->c_str(), &a0);
            inet_pton(AF_INET, (++p)->c_str(), &a1);
            auto port = std::stod(*++p);
            h.add_remote(a0, port, a1);
            p = commands.begin();
            std::cout << "add remote " << *++p << " at " << *++p << ':' << *++p << std::endl;
            return true;
        }
        if (*p == "route")
        {
            in_addr a0, a1;
            inet_pton(AF_INET, (++p)->c_str(), &a0);
            inet_pton(AF_INET, (++p)->c_str(), &a1);
            auto distance = std::stod(*++p);
            h.add_route(a0, a1, distance);
            p = commands.begin();
            std::cout << "add route " << *++p << " via " << *++p << std::endl;
            return true;
        }
        return false;

    default:
        return false;
    }
}

int main(int argc, char *argv[])
{
    std::cout << argc << " arguments:" << std::endl;
    for (auto i = 0; i < argc; ++i)
        std::cout << "  " << i << ": " << argv[i] << std::endl;
    std::cout << std::endl;

    if (argc < 3)
        return 1;

    in_addr address;
    inet_pton(AF_INET, argv[2], &address);
    host_t host(argv[1], address);

    enum
    {
        none,
        bind,
        remote,
        route
    } state = none;
    for (auto p = argv + 3; p < argv + argc; ++p)
    {
        if (std::strcmp(*p, "bind") == 0)
            state = bind;
        else if (std::strcmp(*p, "remote") == 0)
            state = remote;
        else if (std::strcmp(*p, "route") == 0)
            state = route;
        else
            switch (state)
            {
            case none:
                std::cout << "Argument " << *p << " is ignored." << std::endl;
                break;
            case bind:
            {
                auto index = std::stod(*p++);
                auto port = std::stod(*p);
                while (!host.bind(index, port))
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                std::cout << "bind device[" << index << "] with port " << port << std::endl;
                break;
            }
            case remote:
            {
                in_addr a0, a1;
                inet_pton(AF_INET, *p++, &a0);
                inet_pton(AF_INET, *p++, &a1);
                auto port = std::stod(*p);
                host.add_remote(a0, port, a1);
                std::cout << "add remote " << p[-2] << " at " << p[-1] << ':' << *p << std::endl;
                break;
            }
            case route:
            {
                in_addr a0, a1;
                inet_pton(AF_INET, *p++, &a0);
                inet_pton(AF_INET, *p++, &a1);
                auto distance = std::stod(*p);
                host.add_route(a0, a1, distance);
                std::cout << "add route " << p[-2] << " via " << p[-1] << std::endl;
                break;
            }
            }
    }

    auto forwarding = std::thread([&host] {
        uint8_t buffer[2048];
        host.forward(buffer, sizeof(buffer));
    });
    auto receiving = std::thread([&host] {
        uint8_t buffer[2048];
        host.receive(buffer, sizeof(buffer));
    });

    cin_to_list([&host](auto commands) {
        if (!script(host, commands))
            std::cout << "known command" << std::endl;
    });

    return 0;
}
