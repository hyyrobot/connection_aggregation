#include "../host_t.h"

#include "../ERRNO_MACRO.h"

#include <netinet/ip.h>
#include <unistd.h>

namespace autolabor::connection_aggregation
{
    void host_t::yell(in_addr dst)
    {
        fd_guard_t temp(socket(AF_UNIX, SOCK_DGRAM, 0));
        if (!dst.s_addr)
        {
            constexpr static uint8_t msg = YELL;
            sendto(temp, &msg, sizeof(msg), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));
        }
        else
        {
            uint8_t msg[5]{SEND_HANDSHAKE};
            *reinterpret_cast<in_addr *>(msg + 1) = dst;
            sendto(temp, &msg, sizeof(msg), MSG_WAITALL, reinterpret_cast<sockaddr *>(&_address_un), sizeof(sockaddr_un));
        }
    }

    void host_t::send_void(in_addr dst, bool on_demand)
    {
        constexpr static uint8_t nothing = 0;
        // 未指定具体目标，向所有邻居发送握手
        if (!dst.s_addr)
            for (auto &[d, r] : _remotes)
                for (auto s : r.handshake(on_demand))
                    _threads.launch([=, this] { send_single(s, &nothing, 1); });
        // 向指定的邻居发送握手
        else
        {
            auto p = _remotes.find(dst.s_addr);
            if (p != _remotes.end())
                for (auto s : p->second.handshake(on_demand))
                    _threads.launch([=, this] { send_single(s, &nothing, 1); });
        }
    }

    size_t host_t::send_strand(in_addr dst, const uint8_t *buffer, size_t size)
    {
        auto &r = _remotes.at(dst.s_addr);
        // 查询路由表
        auto next = r.next().s_addr;
        auto sendings = next ? _remotes.at(next).forward()
                             : r.forward();
        // 发送
        if (sendings.size() > 2 && _threads.size() > 2)
        {
            std::vector<std::future<bool>> results(sendings.size());
            std::transform(sendings.begin(), sendings.end(), results.begin(),
                           [&](auto s) {
                               return _threads.submit([=, this] {
                                   return send_single(s, buffer, size);
                               });
                           });
            for (auto &f : results)
                f.wait();
        }
        else
            for (auto s : sendings)
                send_single(s, buffer, size);
        return sendings.size();
    }

    // 通过单独的连接发送
    // NOTICE 不要在这里访问 _remotes
    bool host_t::send_single(
        sending_t sending,
        const uint8_t *buffer,
        size_t size)
    {
        auto type = *reinterpret_cast<const type1_t *>(buffer);
        type.state = sending.state;
        sockaddr_in remote{
            .sin_family = AF_INET,
            .sin_port = sending.port,
            .sin_addr = sending.actual,
        };
        iovec iov[]{
            {.iov_base = &_address, .iov_len = sizeof(in_addr)},
            {.iov_base = &type, .iov_len = 1},
            {.iov_base = (void *)(buffer + 1), .iov_len = size - 1},
        };
        msghdr msg{
            .msg_name = &remote,
            .msg_namelen = sizeof(remote),
            .msg_iov = iov,
            .msg_iovlen = sizeof(iov) / sizeof(iovec),
        };

        auto result = _devices.at(sending.index).send(&msg) == sizeof(in_addr) + size;
        if (!result)
            THROW_ERRNO(__FILE__, __LINE__, "send msg from " << sending.index);
        return result;
    }

} // namespace autolabor::connection_aggregation