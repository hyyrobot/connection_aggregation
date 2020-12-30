#ifndef ERRNO_MACRO_H
#define ERRNO_MACRO_H

#include <sstream>
#include <cstring>

#define THROW_ERRNO(FILE, LINE, WHAT)                          \
    {                                                          \
        std::stringstream builder;                             \
        auto why = errno;                                      \
        builder << FILE << ':' << LINE                         \
                << ": exception: Failed to " << WHAT           \
                << ": " << strerror(why) << '(' << why << ')'; \
        throw std::runtime_error(builder.str());               \
    }

#endif // ERRNO_MACRO_H