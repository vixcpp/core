#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <cstddef>

#include <vix/http/cache/CachePolicy.hpp>
#include <vix/http/cache/CacheStore.hpp>
#include <vix/http/cache/CacheContext.hpp>

namespace vix::vhttp::cache
{

    class Cache
    {
    public:
        Cache(CachePolicy policy,
              std::shared_ptr<CacheStore> store);
        std::optional<CacheEntry> get(const std::string &key,
                                      std::int64_t now_ms,
                                      CacheContext ctx);
        void put(const std::string &key, const CacheEntry &entry);
        std::size_t prune(std::int64_t now_ms);

    private:
        CachePolicy policy_;
        std::shared_ptr<CacheStore> store_;
    };

} // namespace vix::vhttp::cache
