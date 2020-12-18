#ifndef FD_GUARD_T_H
#define FD_GUARD_T_H

#include <cstdint>
#include <linux/rtnetlink.h>

namespace autolabor::connection_aggregation
{
    struct fd_guard_t
    {
        // RAII

        fd_guard_t(int = 0);
        ~fd_guard_t();

        // move

        fd_guard_t(fd_guard_t &&) noexcept;
        fd_guard_t &operator=(fd_guard_t &&) noexcept;

        // delete clone

        fd_guard_t(const fd_guard_t &) = delete;
        fd_guard_t &operator=(const fd_guard_t &) = delete;

        // use as int

        operator int() const;

    private:
        int _fd;
    };

    // 创建并绑定关于特定消息组的 netlink 套接字
    fd_guard_t bind_netlink(uint32_t);
    
} // namespace autolabor::connection_aggregation

#endif // FD_GUARD_T_H