#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>

#include <vix/http/cache/CacheStore.hpp>

namespace vix::http::cache
{

    class FileStore final : public CacheStore
    {
    public:
        struct Config
        {
            std::filesystem::path file_path{"./.vix/cache_http.json"};
            bool pretty_json{false};
        };

        explicit FileStore(Config cfg);
        void put(const std::string &key, const CacheEntry &entry) override;
        std::optional<CacheEntry> get(const std::string &key) override;
        void erase(const std::string &key) override;
        void clear() override;

    private:
        void load_();
        void flush_();

    private:
        Config cfg_;
        bool loaded_{false};
        std::mutex mu_;
        std::unordered_map<std::string, CacheEntry> map_;
    };

} // namespace vix::http::cache
