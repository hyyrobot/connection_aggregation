#include "host_t.h"

#include <arpa/inet.h>
#include <list>
#include <thread>
#include <iostream>
#include <sstream>
#include <functional>

using namespace autolabor::connection_aggregation;
using namespace std::chrono_literals;

using func_t = std::function<bool(std::list<std::string>)>;
func_t operator<<(func_t, std::istream &);

bool script(host_t &, const std::list<std::string> &);

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
    host_t h(argv[1], address);

    std::thread([&h] {
        uint8_t buffer[4096];
        h.receive(buffer, sizeof(buffer));
    }).detach();

    std::stringstream builder;
    for (auto i = 3; i < argc; ++i)
        builder << argv[i] << ' ';

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    [&h](auto c) { return script(h, c); } << builder << std::cin;

    return 0;
}

func_t operator<<(func_t func, std::istream &in)
{
    std::list<std::string> commands;
    std::string c;

    while (true)
    {
        // 逐词解析
        if (!(in >> c))
            return func;
        while (true)
        {
            // 分号断句
            auto x = c.find(';');
            // 句子未完
            if (x >= c.size())
            {
                commands.push_back(c);
                break;
            }
            // 句子完结
            if (x > 0)
                commands.push_back(c.substr(0, x));
            // 响应
            if (!func(std::move(commands)))
                return func;
            commands.clear();
            // 恢复剩余成分继续解析
            if (x + 1 < c.size())
                c = c.substr(x + 1);
            else
                break;
        }
    }
}

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
            h.print();
            return true;
        }
        if (*p == "yell")
        {
            h.yell();
            std::this_thread::sleep_for(200ms);
            h.print();
            return true;
        }
        break;

    case 2:
        if (*p++ == "shakehand")
        {
            in_addr a{};
            inet_pton(AF_INET, p->data(), &a);
            h.yell(a);
            std::this_thread::sleep_for(200ms);
            h.print();
            return true;
        }
        break;

    case 3:
        if (*p == "bind")
        {
            auto index = std::stod(*++p);
            auto port = std::stod(*++p);
            h.bind(index, port);
            return true;
        }
        break;

    case 4:
        if (*p == "remote")
        {
            in_addr a0, a1;
            inet_pton(AF_INET, (++p)->c_str(), &a0);
            inet_pton(AF_INET, (++p)->c_str(), &a1);
            auto port = std::stod(*++p);
            h.add_remote(a0, a1, port);
            p = commands.begin();
            std::cout << "add remote " << *++p << " at " << *++p << ':' << *++p << std::endl;
            return true;
        }
        break;
    }
    std::cout << "unknown command" << std::endl;
    return true;
}
