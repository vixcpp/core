#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <vix/http/cache/CachePolicy.hpp>
#include <vix/http/cache/CacheStore.hpp>

namespace vix::http::cache
{

    class Cache
    {
    public:
        Cache(CachePolicy policy,
              std::shared_ptr<CacheStore> store);

        std::optional<CacheEntry> get(const std::string &key, std::int64_t now_ms,
                                      bool network_ok);

        void put(const std::string &key, const CacheEntry &entry);

    private:
        CachePolicy policy_;
        std::shared_ptr<CacheStore> store_;
    };

} // namespace vix::http::cache
