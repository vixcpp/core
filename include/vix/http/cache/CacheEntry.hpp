#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace vix::vhttp::cache
{

    struct CacheEntry
    {
        int status{200};
        std::string body;
        std::unordered_map<std::string, std::string> headers;

        std::int64_t created_at_ms{0};
    };

} // namespace vix::vhttp::cache
