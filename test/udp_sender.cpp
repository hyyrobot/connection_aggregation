#include <arpa/inet.h>

#include <iostream>
#include <sstream>
#include <thread>

int main()
{
    auto udp = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in remote{.sin_family = AF_INET, .sin_port = 12345};
    inet_pton(AF_INET, "10.0.0.1", &remote.sin_addr);

    for (auto i = 0;; ++i)
    {
        std::stringstream builder;
        builder << i << '\t' << "Hello, world!";
        auto text = builder.str();
        sendto(udp, text.c_str(), text.size() + 1, MSG_WAITALL, (sockaddr *)&remote, sizeof(remote));

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(.02s);
    }

    return 0;
}
