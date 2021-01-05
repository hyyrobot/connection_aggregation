#include <netinet/in.h>
#include <iostream>

int main()
{
    auto udp = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local{.sin_family = AF_INET, .sin_port = 12345};
    bind(udp, (sockaddr *)&local, sizeof(local));
    while (true)
    {
        uint8_t buffer[1024];
        recv(udp, buffer, sizeof(buffer), MSG_WAITALL);
        std::cout << buffer << std::endl;
    }

    return 0;
}
