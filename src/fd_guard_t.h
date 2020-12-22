#ifndef FD_GUARD_T_H
#define FD_GUARD_T_H

#include <cstdint>
#include <linux/rtnetlink.h>

namespace autolabor::connection_aggregation
{
    /** RAII 管理文件描述符 */
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

} // namespace autolabor::connection_aggregation

#endif // FD_GUARD_T_H