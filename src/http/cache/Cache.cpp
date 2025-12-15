#include <vix/http/cache/Cache.hpp>

namespace vix::http::cache
{

    Cache::Cache(CachePolicy policy,
                 std::shared_ptr<CacheStore> store)
        : policy_(policy), store_(std::move(store))
    {
    }

    std::optional<CacheEntry>
    Cache::get(const std::string &key, std::int64_t now_ms, bool network_ok)
    {
        auto e = store_->get(key);
        if (!e)
            return std::nullopt;

        const auto age = now_ms - e->created_at_ms;

        if (policy_.is_fresh(age))
        {
            return e;
        }

        if (!network_ok && policy_.allow_stale_offline(age))
        {
            return e;
        }

        if (network_ok == false && policy_.allow_stale_error(age))
        {
            return e;
        }

        return std::nullopt;
    }

    void Cache::put(const std::string &key, const CacheEntry &entry)
    {
        store_->put(key, entry);
    }

} // namespace vix::http::cache
