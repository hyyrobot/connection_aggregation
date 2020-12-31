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

            auto index = static_cast<device_index_t>(event.data.u32);
            sockaddr_in remote{.sin_family = AF_INET};
            size_t n;
            {
                READ_LOCK(_device_mutex);
                auto p = _devices.find(index);
                if (p == _devices.end())
                    continue;
                iovec iov{.iov_base = buffer, .iov_len = size};
                msghdr msg{
                    .msg_name = &remote,
                    .msg_namelen = sizeof(remote),
                    .msg_iov = &iov,
                    .msg_iovlen = 1,
                };
                n = p->second.receive(&msg);
            }

            connection_key_union _union{.pair{.src_index = index, .dst_port = remote.sin_port}};
            auto source = reinterpret_cast<in_addr *>(buffer);       // 解出源虚拟地址
            auto type = reinterpret_cast<pack_type_t *>(source + 1); // 解出类型字节
            add_remote(*source, remote.sin_port, remote.sin_addr);
            {
                READ_LOCK(_srand_mutex);
                auto s = get_srand(remote.sin_addr);
                if (!s)
                    continue;
                read_lock lc(s->connection_mutex);
                s->connections.at(_union.key).received_once(type->state);
            }

            return n;
        }
    }
} // namespace autolabor::connection_aggregation