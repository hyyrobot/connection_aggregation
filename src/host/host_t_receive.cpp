#include "../host_t.h"

#include "../ERRNO_MACRO.h"
#include <sys/epoll.h>

namespace autolabor::connection_aggregation
{
    size_t host_t::receive(uint8_t *buffer, size_t size)
    {
        epoll_event event;
        while (true)
        {
            auto what = epoll_wait(_epoll, &event, 1, -1);
            if (what == 0)
                continue;
            if (what < 0)
                THROW_ERRNO(__FILE__, __LINE__, "wait epoll");
            auto device = reinterpret_cast<device_t *>(event.data.ptr);

            sockaddr_in remote{.sin_family = AF_INET};
            iovec iov[]{{.iov_base = buffer, .iov_len = size}};
            msghdr msg{
                .msg_name = &remote,
                .msg_namelen = sizeof(remote),
                .msg_iov = iov,
                .msg_iovlen = sizeof(iov) / sizeof(iovec),
            };
            return device->receive(&msg);
        }
    }

} // namespace autolabor::connection_aggregation