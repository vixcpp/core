#include <vix/http/cache/FileStore.hpp>

#include <fstream>
#include <nlohmann/json.hpp>

namespace vix::vhttp::cache
{
    using json = nlohmann::json;

    static json to_json(const CacheEntry &e)
    {
        return json{
            {"status", e.status},
            {"body", e.body},
            {"headers", e.headers},
            {"created_at_ms", e.created_at_ms}};
    }

    static CacheEntry from_json(const json &j)
    {
        CacheEntry e;
        e.status = j.value("status", 200);
        e.body = j.value("body", "");
        e.headers = j.value("headers", std::unordered_map<std::string, std::string>{});
        e.created_at_ms = j.value("created_at_ms", 0LL);
        return e;
    }

    FileStore::FileStore(Config cfg) : cfg_(std::move(cfg)) {}

    void FileStore::load_()
    {
        if (loaded_)
            return;
        std::ifstream in(cfg_.file_path);
        if (!in.good())
        {
            loaded_ = true;
            return;
        }

        json root;
        in >> root;
        for (auto &[k, v] : root.items())
        {
            map_[k] = from_json(v);
        }
        loaded_ = true;
    }

    void FileStore::flush_()
    {
        std::filesystem::create_directories(cfg_.file_path.parent_path());
        json root;
        for (auto &[k, v] : map_)
        {
            root[k] = to_json(v);
        }

        std::ofstream out(cfg_.file_path, std::ios::trunc);
        out << (cfg_.pretty_json ? root.dump(2) : root.dump());
    }

    void FileStore::put(const std::string &key, const CacheEntry &entry)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_();
        map_[key] = entry;
        flush_();
    }

    std::optional<CacheEntry> FileStore::get(const std::string &key)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_();
        auto it = map_.find(key);
        if (it == map_.end())
            return std::nullopt;
        return it->second;
    }

    void FileStore::erase(const std::string &key)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_();
        map_.erase(key);
        flush_();
    }

    void FileStore::clear()
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_();
        map_.clear();
        flush_();
    }

} // namespace vix::vhttp::cache
