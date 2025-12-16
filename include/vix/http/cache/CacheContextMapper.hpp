#pragma once

#include <cstdint>

#include <vix/http/cache/CacheContext.hpp>
#include <vix/sync/NetworkProbe.hpp>

namespace vix::http::cache
{

    /**
     * Minimal classification of request outcomes for caching decisions.
     * - Ok: request succeeded (HTTP 2xx/3xx/4xx/5xx all reached server)
     * - NetworkError: request failed due to network (timeout, DNS, reset...)
     *
     * NOTE: HTTP 5xx is NOT a network error: the network worked.
     */
    enum class RequestOutcome
    {
        Ok,
        NetworkError
    };

    /**
     * Build CacheContext only from NetworkProbe.
     * If probe says offline => ctx.offline=true
     */
    inline CacheContext contextFromProbe(const vix::sync::NetworkProbe &probe,
                                         std::int64_t now_ms)
    {
        CacheContext ctx{};
        if (!probe.isOnline(now_ms))
        {
            ctx.offline = true;
        }
        return ctx;
    }

    /**
     * Combine NetworkProbe + request outcome.
     * - offline is driven by probe
     * - network_error is driven by the request outcome
     */
    inline CacheContext contextFromProbeAndOutcome(const vix::sync::NetworkProbe &probe,
                                                   std::int64_t now_ms,
                                                   RequestOutcome outcome)
    {
        CacheContext ctx = contextFromProbe(probe, now_ms);
        if (outcome == RequestOutcome::NetworkError)
        {
            ctx.network_error = true;
        }
        return ctx;
    }

    /**
     * Utility helpers
     */
    inline CacheContext contextOffline() noexcept { return CacheContext::Offline(); }
    inline CacheContext contextOnline() noexcept { return CacheContext::Online(); }
    inline CacheContext contextNetworkError() noexcept { return CacheContext::NetworkError(); }

} // namespace vix::http::cache
