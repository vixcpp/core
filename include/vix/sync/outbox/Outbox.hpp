#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vix/sync/RetryPolicy.hpp>
#include <vix/sync/Operation.hpp>
#include <vix/sync/outbox/OutboxStore.hpp>

namespace vix::sync::outbox
{

    class Outbox
    {
    public:
        struct Config
        {
            std::string owner{"vix-sync"}; // identifies the sync engine instance
            RetryPolicy retry{};
            bool auto_generate_ids{true};
            bool auto_generate_idempotency_key{true};
        };

        Outbox(Config cfg, std::shared_ptr<OutboxStore> store);
        std::string enqueue(vix::sync::Operation op, std::int64_t now_ms);
        std::vector<vix::sync::Operation> peek_ready(std::int64_t now_ms, std::size_t limit = 50);
        bool claim(const std::string &id, std::int64_t now_ms);
        bool complete(const std::string &id, std::int64_t now_ms);
        bool fail(const std::string &id, const std::string &error, std::int64_t now_ms);
        std::shared_ptr<OutboxStore> store() const noexcept { return store_; }
        const Config &config() const noexcept { return cfg_; }

    private:
        static std::string make_id();
        static std::string make_idempotency_key();

    private:
        Config cfg_;
        std::shared_ptr<OutboxStore> store_;
    };

} // namespace vix::sync::outbox
