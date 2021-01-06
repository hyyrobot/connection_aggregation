#include "host_t.h"
#include "cin_to_list.h"

#include <arpa/inet.h>
#include <cstring>
#include <list>
#include <thread>
#include <iostream>
#include <sstream>

using namespace autolabor::connection_aggregation;
using namespace std::chrono_literals;

bool script(host_t &h, const std::list<std::string> &commands)
{
    auto p = commands.begin();
    switch (commands.size())
    {
    case 1:
        if (*p == "exit")
            return false;
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
        std::cout << "known command" << std::endl;
        return true;

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
        std::cout << "known command" << std::endl;
        return true;

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
        std::cout << "known command" << std::endl;
        return true;

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
        std::cout << "known command" << std::endl;
        return true;

    default:
        std::cout << "known command" << std::endl;
        return true;
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

    std::thread([&host] {
        uint8_t buffer[4096];
        host.receive(buffer, sizeof(buffer));
    }).detach();

    std::stringstream builder;
    for (auto i = 3; i < argc; ++i)
        builder << argv[i] << ' ';
        
    stream_to_list(builder, [&host](auto commands) { return script(host, commands); });
    stream_to_list(std::cin, [&host](auto commands) { return script(host, commands); });

    return 0;
}
