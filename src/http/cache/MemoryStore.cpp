#include <vix/http/cache/MemoryStore.hpp>

namespace vix::http::cache
{

    void MemoryStore::put(const std::string &key, const CacheEntry &entry)
    {
        std::lock_guard<std::mutex> lk(mu_);
        map_[key] = entry;
    }

    std::optional<CacheEntry> MemoryStore::get(const std::string &key)
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = map_.find(key);
        if (it == map_.end())
            return std::nullopt;
        return it->second;
    }

    void MemoryStore::erase(const std::string &key)
    {
        std::lock_guard<std::mutex> lk(mu_);
        map_.erase(key);
    }

    void MemoryStore::clear()
    {
        std::lock_guard<std::mutex> lk(mu_);
        map_.clear();
    }

} // namespace vix::http::cache
