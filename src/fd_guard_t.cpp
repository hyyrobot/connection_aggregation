#include "fd_guard_t.h"

#include "ERRNO_MACRO.h"

#include <unistd.h> // close

namespace autolabor::connection_aggregation
{

    fd_guard_t::fd_guard_t(int fd) : _fd(fd)
    {
        if (fd < 0)
            THROW_ERRNO(__FILE__, __LINE__, "guard fd " << fd);
    }

    fd_guard_t::fd_guard_t(fd_guard_t &&origin) noexcept : _fd(origin._fd)
    {
        origin._fd = 0;
    }

    fd_guard_t &fd_guard_t::operator=(fd_guard_t &&origin) noexcept
    {
        if (_fd)
            close(_fd);
        _fd = origin._fd;
        origin._fd = 0;
        return *this;
    }

    fd_guard_t::~fd_guard_t()
    {
        if (_fd)
            close(_fd);
    }

    fd_guard_t::operator int() const
    {
        return _fd;
    }

} // namespace autolabor::connection_aggregation