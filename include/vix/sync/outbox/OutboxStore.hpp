#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <vix/sync/Operation.hpp>

namespace vix::sync::outbox
{

    struct ListOptions
    {
        std::size_t limit{50};
        std::int64_t now_ms{0};
        // Only return ops whose next_retry_at_ms <= now_ms
        bool only_ready{true};
        // If true, include InFlight (usually false)
        bool include_inflight{false};
    };

    class OutboxStore
    {
    public:
        virtual ~OutboxStore() = default;
        virtual void put(const vix::sync::Operation &op) = 0;
        virtual std::optional<vix::sync::Operation> get(const std::string &id) = 0;
        // Returns candidates for sending (usually Pending/Failed ready)
        virtual std::vector<vix::sync::Operation> list(const ListOptions &opt) = 0;
        // Claim op for sending: set status=InFlight, update timestamps, store owner.
        virtual bool claim(const std::string &id, const std::string &owner, std::int64_t now_ms) = 0;
        virtual bool mark_done(const std::string &id, std::int64_t now_ms) = 0;
        virtual bool mark_failed(const std::string &id,
                                 const std::string &error,
                                 std::int64_t now_ms,
                                 std::int64_t next_retry_at_ms) = 0;

        // Optional cleanup
        virtual std::size_t prune_done(std::int64_t older_than_ms) = 0;
    };

} // namespace vix::sync::outbox
