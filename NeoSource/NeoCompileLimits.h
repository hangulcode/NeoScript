#pragma once

#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace NeoScript
{
// Check before narrowing or writing. The parser attaches the source location;
// the public compiler also catches failures during final image serialization.
class CompileLimitError : public std::runtime_error
{
public:
    CompileLimitError(const char* item, int64_t value, int64_t minimum, int64_t maximum)
        : std::runtime_error(std::string(item) + " limit exceeded (" + std::to_string(value)
            + "; range " + std::to_string(minimum) + ".." + std::to_string(maximum) + ")") {}
};

inline int64_t CheckCompileRange(const char* item, int64_t value, int64_t minimum, int64_t maximum)
{
    if (value < minimum || value > maximum)
        throw CompileLimitError(item, value, minimum, maximum);
    return value;
}

inline short CompileShort(const char* item, int value)
{
    return static_cast<short>(CheckCompileRange(item, value, SHRT_MIN, SHRT_MAX));
}
}
