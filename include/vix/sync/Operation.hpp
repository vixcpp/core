#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace vix::sync
{

    enum class OperationStatus : uint8_t
    {
        Pending = 0,
        InFlight,
        Done,
        Failed,
        PermanentFailed
    };

    struct Operation
    {
        std::string id;
        std::string kind;
        std::string target;
        std::string payload;
        std::string idempotency_key;

        std::int64_t created_at_ms{0};
        std::int64_t updated_at_ms{0};
        std::uint32_t attempt{0};
        std::int64_t next_retry_at_ms{0};

        OperationStatus status{OperationStatus::Pending};
        std::string last_error;

        bool is_done() const noexcept { return status == OperationStatus::Done; }
        bool is_pending() const noexcept { return status == OperationStatus::Pending; }
        bool is_failed() const noexcept { return status == OperationStatus::Failed; }

        void fail(std::string err, std::int64_t now_ms)
        {
            last_error = std::move(err);
            status = OperationStatus::Failed;
            updated_at_ms = now_ms;
        }

        void done(std::int64_t now_ms)
        {
            status = OperationStatus::Done;
            updated_at_ms = now_ms;
            last_error.clear();
        }
    };

} // namespace vix::sync
