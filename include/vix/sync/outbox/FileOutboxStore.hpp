#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vix/sync/Operation.hpp>
#include <vix/sync/outbox/OutboxStore.hpp>

namespace vix::sync::outbox
{

    class FileOutboxStore final : public OutboxStore
    {
    public:
        struct Config
        {
            std::filesystem::path file_path{"./.vix/outbox.json"};
            bool pretty_json{false};
            bool fsync_on_write{false};
        };

        explicit FileOutboxStore(Config cfg);

        void put(const vix::sync::Operation &op) override;
        std::optional<vix::sync::Operation> get(const std::string &id) override;
        std::vector<vix::sync::Operation> list(const ListOptions &opt) override;

        bool claim(const std::string &id, const std::string &owner, std::int64_t now_ms) override;
        bool mark_done(const std::string &id, std::int64_t now_ms) override;

        bool mark_failed(const std::string &id,
                         const std::string &error,
                         std::int64_t now_ms,
                         std::int64_t next_retry_at_ms) override;

        std::size_t prune_done(std::int64_t older_than_ms) override;

        bool mark_permanent_failed(const std::string &id,
                                   const std::string &error,
                                   std::int64_t now_ms) override;

        std::size_t requeue_inflight_older_than(
            std::int64_t now_ms,
            std::int64_t timeout_ms) override;

    private:
        void load_if_needed_();
        void flush_();

    private:
        Config cfg_;
        std::mutex mu_;
        bool loaded_{false};
        std::unordered_map<std::string, vix::sync::Operation> ops_;
        std::unordered_map<std::string, std::string> owner_;
    };

} // namespace vix::sync::outbox
