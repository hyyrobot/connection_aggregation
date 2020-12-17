#include "ip_table_t.h"

#include <cstring>

bool ip_table_t::push(in_addr ip, const char *host)
{
    auto temp = table1.find(ip.s_addr);
    if (temp != table1.end())
        if (std::strcmp(host, temp->second.data()) == 0)
            return false;
        else
            table2[temp->second].erase(ip.s_addr);
    table1[ip.s_addr] = std::string(host);
    table2[temp->second].insert(ip.s_addr);
    return true;
}

std::string ip_table_t::operator[](in_addr ip) const
{
    auto temp = table1.find(ip.s_addr);
    if (temp != table1.end())
        return temp->second;
    return "";
}

std::unordered_set<in_addr_t> ip_table_t::operator[](const std::string &host) const
{
    auto temp = table2.find(host);
    if (temp != table2.end())
        return temp->second;
    return {};
}
