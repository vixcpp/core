#include <algorithm>
#include <vix/http/cache/LruMemoryStore.hpp>
#include <vix/http/cache/FileStore.hpp>
#include <vix/http/cache/Cache.hpp>
#include <vix/http/cache/HeaderUtil.hpp>

namespace vix::http::cache
{

    Cache::Cache(CachePolicy policy, std::shared_ptr<CacheStore> store)
        : policy_(policy), store_(std::move(store)) {}

    std::optional<CacheEntry> Cache::get(const std::string &key,
                                         std::int64_t now_ms,
                                         CacheContext ctx)
    {
        auto e = store_->get(key);
        if (!e)
            return std::nullopt;

        const auto age = now_ms - e->created_at_ms;

        if (policy_.is_fresh(age))
        {
            return e;
        }

        if (ctx.offline && policy_.allow_stale_offline(age))
        {
            return e;
        }

        if (ctx.network_error && policy_.allow_stale_error(age))
        {
            return e;
        }

        return std::nullopt;
    }

    void Cache::put(const std::string &key, const CacheEntry &entry)
    {
        CacheEntry e = entry;
        HeaderUtil::normalizeInPlace(e.headers);
        store_->put(key, e);
    }

    static std::int64_t max_age_for_policy(const CachePolicy &p)
    {
        std::int64_t m = p.ttl_ms;
        if (p.allow_stale_if_error)
            m = std::max(m, p.stale_if_error_ms);
        if (p.allow_stale_if_offline)
            m = std::max(m, p.stale_if_offline_ms);
        return m;
    }

    std::size_t Cache::prune(std::int64_t now_ms)
    {
        const std::int64_t max_age = max_age_for_policy(policy_);

        if (auto *lru = dynamic_cast<LruMemoryStore *>(store_.get()))
        {
            return lru->eraseIf([&](const CacheEntry &e)
                                {
                const auto age = now_ms - e.created_at_ms;
                return age > max_age; });
        }

        if (auto *fs = dynamic_cast<FileStore *>(store_.get()))
        {
            return fs->eraseIf([&](const CacheEntry &e)
                               {
                const auto age = now_ms - e.created_at_ms;
                return age > max_age; });
        }

        return 0;
    }

} // namespace vix::http::cache
