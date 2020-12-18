#include <netinet/ip.h>
#include <arpa/inet.h>

#include "net_devices_t.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

int main()
{
    using namespace autolabor::connection_aggregation;

    net_devices_t devices;
    in_addr address_remote;
    inet_pton(AF_INET, "192.168.18.182", &address_remote);
    sockaddr_in remote{
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr = address_remote,
    };

    ip header{
        .ip_hl = 5 + 4,
        .ip_v = 4,
        .ip_len = 42,
        .ip_id = 4,
        .ip_off = 0,
        .ip_ttl = 64,
        .ip_p = 3,
        .ip_dst = address_remote,
    };
    struct
    {
        char host[14];
        uint16_t id;
    } extra{
        .id = 0,
    };
    std::strcpy(extra.host, "robot");
    iovec iov[]{
        {.iov_base = &header, .iov_len = sizeof(header)},
        {.iov_base = &extra, .iov_len = sizeof(extra)},
        {.iov_base = (void *)"Hello", .iov_len = 6},
    };
    msghdr msg{
        .msg_name = &remote,
        .msg_namelen = sizeof(remote),
        .msg_iov = iov,
        .msg_iovlen = sizeof(iov) / sizeof(iovec),
    };

    while (true)
    {
        auto result = devices.send(&msg);
        std::cout << ++extra.id << ": " << std::endl;
        for (auto pair : result)
            std::cout << '<' << devices[pair.first] << ": " << pair.second << '>' << std::endl;

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.5s);
    }

    return 0;
}
