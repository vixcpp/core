#include <vix/sync/outbox/FileOutboxStore.hpp>

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace vix::sync::outbox
{
    using json = nlohmann::json;

    static json op_to_json(const vix::sync::Operation &op)
    {
        return json{
            {"id", op.id},
            {"kind", op.kind},
            {"target", op.target},
            {"payload", op.payload},
            {"idempotency_key", op.idempotency_key},
            {"created_at_ms", op.created_at_ms},
            {"updated_at_ms", op.updated_at_ms},
            {"attempt", op.attempt},
            {"next_retry_at_ms", op.next_retry_at_ms},
            {"status", static_cast<int>(op.status)},
            {"last_error", op.last_error},
        };
    }

    static vix::sync::Operation op_from_json(const json &j)
    {
        vix::sync::Operation op;
        op.id = j.value("id", "");
        op.kind = j.value("kind", "");
        op.target = j.value("target", "");
        op.payload = j.value("payload", "");
        op.idempotency_key = j.value("idempotency_key", "");
        op.created_at_ms = j.value("created_at_ms", 0LL);
        op.updated_at_ms = j.value("updated_at_ms", 0LL);
        op.attempt = j.value("attempt", 0u);
        op.next_retry_at_ms = j.value("next_retry_at_ms", 0LL);
        op.status = static_cast<vix::sync::OperationStatus>(j.value("status", 0));
        op.last_error = j.value("last_error", "");
        return op;
    }

    FileOutboxStore::FileOutboxStore(Config cfg) : cfg_(std::move(cfg)) {}

    void FileOutboxStore::load_if_needed_()
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

        auto ops = root.value("ops", json::object());
        for (auto it = ops.begin(); it != ops.end(); ++it)
        {
            vix::sync::Operation op = op_from_json(it.value());
            ops_[op.id] = std::move(op);
        }

        auto owners = root.value("owners", json::object());
        for (auto it = owners.begin(); it != owners.end(); ++it)
        {
            owner_[it.key()] = it.value().get<std::string>();
        }

        loaded_ = true;
    }

    void FileOutboxStore::flush_()
    {
        std::filesystem::create_directories(cfg_.file_path.parent_path());

        json root;
        root["version"] = 1;

        json ops = json::object();
        for (const auto &[id, op] : ops_)
        {
            ops[id] = op_to_json(op);
        }
        root["ops"] = std::move(ops);

        json owners = json::object();
        for (const auto &[id, o] : owner_)
        {
            owners[id] = o;
        }
        root["owners"] = std::move(owners);

        std::ofstream out(cfg_.file_path, std::ios::trunc);
        if (!out.good())
            throw std::runtime_error("FileOutboxStore: cannot write outbox file");

        if (cfg_.pretty_json)
            out << root.dump(2);
        else
            out << root.dump();
    }

    void FileOutboxStore::put(const vix::sync::Operation &op)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();
        ops_[op.id] = op;
        flush_();
    }

    std::optional<vix::sync::Operation> FileOutboxStore::get(const std::string &id)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();
        auto it = ops_.find(id);
        if (it == ops_.end())
            return std::nullopt;
        return it->second;
    }

    std::vector<vix::sync::Operation> FileOutboxStore::list(const ListOptions &opt)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();

        std::vector<vix::sync::Operation> out;
        out.reserve(opt.limit);

        for (const auto &[id, op] : ops_)
        {
            const bool is_ready = (op.next_retry_at_ms <= opt.now_ms);
            const bool is_inflight = (op.status == vix::sync::OperationStatus::InFlight);

            if (!opt.include_inflight && is_inflight)
                continue;
            if (opt.only_ready && !is_ready)
                continue;

            if (op.status == vix::sync::OperationStatus::Done)
                continue;

            if (op.status == vix::sync::OperationStatus::PermanentFailed)
                continue;

            out.push_back(op);
            if (out.size() >= opt.limit)
                break;
        }
        return out;
    }

    bool FileOutboxStore::claim(const std::string &id, const std::string &owner, std::int64_t now_ms)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();

        auto it = ops_.find(id);
        if (it == ops_.end())
            return false;

        auto &op = it->second;

        // Only claim if not already in-flight/done
        if (op.status == vix::sync::OperationStatus::Done)
            return false;
        if (op.status == vix::sync::OperationStatus::InFlight)
            return false;

        op.status = vix::sync::OperationStatus::InFlight;
        op.updated_at_ms = now_ms;
        owner_[id] = owner;
        flush_();
        return true;
    }

    bool FileOutboxStore::mark_done(const std::string &id, std::int64_t now_ms)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();

        auto it = ops_.find(id);
        if (it == ops_.end())
            return false;

        auto &op = it->second;
        op.status = vix::sync::OperationStatus::Done;
        op.updated_at_ms = now_ms;
        op.last_error.clear();

        owner_.erase(id);
        flush_();
        return true;
    }

    bool FileOutboxStore::mark_failed(const std::string &id,
                                      const std::string &error,
                                      std::int64_t now_ms,
                                      std::int64_t next_retry_at_ms)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();

        auto it = ops_.find(id);
        if (it == ops_.end())
            return false;

        auto &op = it->second;
        op.status = vix::sync::OperationStatus::Failed;
        op.last_error = error;
        op.updated_at_ms = now_ms;
        op.next_retry_at_ms = next_retry_at_ms;

        owner_.erase(id);
        flush_();
        return true;
    }

    std::size_t FileOutboxStore::prune_done(std::int64_t older_than_ms)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();

        std::size_t removed = 0;
        for (auto it = ops_.begin(); it != ops_.end();)
        {
            const auto &op = it->second;
            if (op.status == vix::sync::OperationStatus::Done && op.updated_at_ms <= older_than_ms)
            {
                owner_.erase(it->first);
                it = ops_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }

        if (removed > 0)
            flush_();
        return removed;
    }

    bool FileOutboxStore::mark_permanent_failed(const std::string &id,
                                                const std::string &error,
                                                std::int64_t now_ms)
    {
        std::lock_guard<std::mutex> lk(mu_);
        load_if_needed_();

        auto it = ops_.find(id);
        if (it == ops_.end())
            return false;

        auto &op = it->second;
        op.status = vix::sync::OperationStatus::PermanentFailed;
        op.last_error = error;
        op.updated_at_ms = now_ms;
        op.next_retry_at_ms = now_ms;

        owner_.erase(id);
        flush_();
        return true;
    }

} // namespace vix::sync::outbox
