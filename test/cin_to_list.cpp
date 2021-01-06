#include "cin_to_list.h"

#include <string>

void stream_to_list(std::istream &in, const std::function<bool(std::list<std::string> &&)> &func)
{
    std::list<std::string> commands;

    while (true)
    {
        std::string c;
        if (!(in >> c))
            return;
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
            if (!func(std::move(commands)))
                return;
            commands.clear();
            if (x + 1 < c.size())
                c = c.substr(x + 1);
            else
                break;
        }
    }
}
