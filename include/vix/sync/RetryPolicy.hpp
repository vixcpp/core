#pragma once

#include <cstdint>
#include <algorithm>

namespace vix::sync
{

    struct RetryPolicy
    {
        std::uint32_t max_attempts{8};
        std::int64_t base_delay_ms{500};   // 0.5s
        std::int64_t max_delay_ms{30'000}; // 30s
        // Exponential factor: delay = base * (2^attempt)
        double factor{2.0};
        // 0.0 = none, 0.2 = +/-20%
        double jitter_ratio{0.2};

        bool can_retry(std::uint32_t attempt) const noexcept
        {
            return attempt < max_attempts;
        }

        std::int64_t compute_delay_ms(std::uint32_t attempt) const noexcept
        {
            // attempt: 0,1,2,... (0 => base)
            double d = static_cast<double>(base_delay_ms);
            for (std::uint32_t i = 0; i < attempt; ++i)
                d *= factor;

            auto delay = static_cast<std::int64_t>(d);
            return std::clamp(delay, base_delay_ms, max_delay_ms);
        }
    };

} // namespace vix::sync
