#ifndef VIX_METRICS_HPP
#define VIX_METRICS_HPP

#include <cstdint>

namespace Vix
{
    struct Metrics
    {
        std::uint64_t pending{0};
        std::uint64_t active{0};
        std::uint64_t timed_out{0};
    };
}

#endif