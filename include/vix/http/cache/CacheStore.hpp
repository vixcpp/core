#pragma once

#include <optional>
#include <string>

#include <vix/http/cache/CacheEntry.hpp>

namespace vix::http::cache
{

    class CacheStore
    {
    public:
        virtual ~CacheStore() = default;
        virtual void put(const std::string &key, const CacheEntry &entry) = 0;
        virtual std::optional<CacheEntry> get(const std::string &key) = 0;
        virtual void erase(const std::string &key) = 0;
        virtual void clear() = 0;
    };

} // namespace vix::http::cache
