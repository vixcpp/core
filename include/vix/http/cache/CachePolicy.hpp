#pragma once

#include <cstdint>

namespace vix::vhttp::cache
{

    struct CachePolicy
    {
        // TTL normal (cache frais)
        std::int64_t ttl_ms{60'000}; // 1 min
        // Accepter une réponse expirée si erreur réseau
        std::int64_t stale_if_error_ms{5 * 60'000}; // 5 min
        // Accepter une réponse expirée si offline
        std::int64_t stale_if_offline_ms{10 * 60'000};
        bool allow_stale_if_error{true};
        bool allow_stale_if_offline{true};

        bool is_fresh(std::int64_t age_ms) const noexcept
        {
            return age_ms <= ttl_ms;
        }

        bool allow_stale_error(std::int64_t age_ms) const noexcept
        {
            return allow_stale_if_error && age_ms <= stale_if_error_ms;
        }

        bool allow_stale_offline(std::int64_t age_ms) const noexcept
        {
            return allow_stale_if_offline && age_ms <= stale_if_offline_ms;
        }
    };

} // namespace vix::vhttp::cache
