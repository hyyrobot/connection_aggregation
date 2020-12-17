#include "net_device/net_device.h"

#include <iostream>

int main()
{
    using namespace autolabor::connection_aggregation;

    auto netlink = bind_netlink(RTMGRP_LINK);
    char name[16]{};
    unsigned index;
    auto tun = register_tun(netlink, name, &index);
    std::cout << name << '(' << index << ')' << std::endl;
    auto list = list_devices(netlink);
    std::cout << list;

    while (true)
        ;
}