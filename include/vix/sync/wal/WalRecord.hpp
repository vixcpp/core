#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vix::sync::wal
{

    enum class RecordType : std::uint8_t
    {
        PutOperation = 1,
        MarkDone = 2,
        MarkFailed = 3,
    };

    struct WalRecord
    {
        std::string id;
        RecordType type{RecordType::PutOperation};
        std::int64_t ts_ms{0};
        std::vector<std::uint8_t> payload;
        std::string error;
        std::int64_t next_retry_at_ms{0};
    };

} // namespace vix::sync::wal
