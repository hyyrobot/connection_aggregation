#ifndef IP_TABLE_T_H
#define IP_TABLE_T_H

#include <netinet/in.h>

#include <unordered_set>
#include <unordered_map>

struct ip_table_t
{
    // 插入一个地址记录
    bool push(in_addr, const char *);

    // 查询地址对应的主机
    std::string operator[](in_addr) const;

    // 查询主机的所有地址
    std::unordered_set<in_addr_t> operator[](const std::string &) const;

private:
    std::unordered_map<in_addr_t, std::string> table1;
    std::unordered_map<std::string, std::unordered_set<in_addr_t>> table2;
};

#endif // IP_TABLE_T_H
