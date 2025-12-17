#pragma once

#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <vix/http/cache/CacheEntry.hpp>
#include <vix/http/cache/CacheStore.hpp>

namespace vix::http::cache
{
    class LruMemoryStore final : public CacheStore
    {
    public:
        struct Config
        {
            std::size_t max_entries{1024};
        };

        explicit LruMemoryStore(Config cfg) : cfg_(cfg) {}

        void put(const std::string &key, const CacheEntry &entry) override
        {
            std::lock_guard<std::mutex> lk(mu_);

            auto it = map_.find(key);
            if (it != map_.end())
            {
                it->second.entry = entry;
                touch_(it);
                return;
            }

            lru_.push_front(key);
            Node n;
            n.entry = entry;
            n.it = lru_.begin();
            map_.emplace(key, std::move(n));

            evictIfNeeded_();
        }

        std::optional<CacheEntry> get(const std::string &key) override
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = map_.find(key);
            if (it == map_.end())
                return std::nullopt;

            touch_(it);
            return it->second.entry;
        }

        void erase(const std::string &key) override
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = map_.find(key);
            if (it == map_.end())
                return;

            lru_.erase(it->second.it);
            map_.erase(it);
        }

        void clear() override
        {
            std::lock_guard<std::mutex> lk(mu_);
            map_.clear();
            lru_.clear();
        }

        // Pour le GC/prune (voir section 4)
        template <typename Pred>
        std::size_t eraseIf(Pred pred)
        {
            std::lock_guard<std::mutex> lk(mu_);
            std::size_t removed = 0;

            for (auto it = map_.begin(); it != map_.end();)
            {
                if (pred(it->second.entry))
                {
                    lru_.erase(it->second.it);
                    it = map_.erase(it);
                    ++removed;
                }
                else
                {
                    ++it;
                }
            }

            return removed;
        }

    private:
        struct Node
        {
            CacheEntry entry{};
            std::list<std::string>::iterator it{};
        };

        void touch_(typename std::unordered_map<std::string, Node>::iterator it)
        {
            lru_.splice(lru_.begin(), lru_, it->second.it);
            it->second.it = lru_.begin();
        }

        void evictIfNeeded_()
        {
            while (map_.size() > cfg_.max_entries && !lru_.empty())
            {
                const std::string &victim = lru_.back();
                auto it = map_.find(victim);
                if (it != map_.end())
                    map_.erase(it);
                lru_.pop_back();
            }
        }

    private:
        Config cfg_;
        std::mutex mu_;
        std::list<std::string> lru_;
        std::unordered_map<std::string, Node> map_;
    };

} // namespace vix::http::cache
