#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>

#include <vix/sync/wal/WalRecord.hpp>

namespace vix::sync::wal
{

    class Wal
    {
    public:
        struct Config
        {
            std::filesystem::path file_path{"./.vix/wal.log"};
            bool fsync_on_write{false};
        };

        explicit Wal(Config cfg);

        std::int64_t append(const WalRecord &rec);

        std::int64_t replay(std::int64_t from_offset,
                            const std::function<void(const WalRecord &)> &on_record);

    private:
        Config cfg_;
    };

} // namespace vix::sync::wal
