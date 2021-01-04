#include "cin_to_list.h"

#include <string>
#include <iostream>

void cin_to_list(const std::function<void(std::list<std::string> &&)> &func)
{
    std::list<std::string> commands;

    while (true)
    {
        if (commands.empty())
        {
            std::cout << ">> ";
            std::cout.flush();
        }
        std::string c;
        std::cin >> c;
        while (true)
        {
            auto x = c.find(';');
            if (x >= c.size())
            {
                commands.push_back(c);
                break;
            }
            if (x > 0)
                commands.push_back(c.substr(0, x));
            func(std::move(commands));
            commands.clear();
            if (x + 1 < c.size())
                c = c.substr(x + 1);
            else
                break;
        }
    }
}