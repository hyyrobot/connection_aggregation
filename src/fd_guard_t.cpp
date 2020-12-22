#include "fd_guard_t.h"

#include <netinet/in.h>
#include <unistd.h> // close

#include <sstream>
#include <cstring>

namespace autolabor::connection_aggregation
{

    fd_guard_t::fd_guard_t(int fd) : _fd(fd)
    {
        if (fd >= 0)
            return;

        std::stringstream builder;
        builder << "Guard an invalid file description: fd = " << fd << ", errno = " << strerror(errno);
        throw std::runtime_error(builder.str());
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