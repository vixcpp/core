#pragma once

namespace vix::http::cache
{

    struct CacheContext
    {
        bool offline{false};       // no network
        bool network_error{false}; // request failed due to network issues

        static CacheContext Online() noexcept { return {}; }
        static CacheContext Offline() noexcept { return {.offline = true}; }
        static CacheContext NetworkError() noexcept { return {.network_error = true}; }
    };

} // namespace vix::http::cache
