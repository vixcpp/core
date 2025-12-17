#pragma once

#include <mutex>
#include <unordered_map>

#include <vix/http/cache/CacheStore.hpp>

namespace vix::http::cache
{

    class MemoryStore final : public CacheStore
    {
    public:
        MemoryStore() : CacheStore{}, mu_{}, map_{} {}

        void put(const std::string &key, const CacheEntry &entry) override;
        std::optional<CacheEntry> get(const std::string &key) override;
        void erase(const std::string &key) override;
        void clear() override;

    private:
        std::mutex mu_;
        std::unordered_map<std::string, CacheEntry> map_;
    };
} // namespace vix::http::cache
