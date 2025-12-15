#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>

#include <vix/sync/wal/WalRecord.hpp>

namespace vix::sync::wal
{

    class WalReader
    {
    public:
        explicit WalReader(std::filesystem::path file_path);
        void seek(std::int64_t offset);
        std::optional<WalRecord> next();
        std::int64_t current_offset() const noexcept { return offset_; }

    private:
        void open_();

    private:
        std::filesystem::path file_path_;
        std::ifstream in_;
        std::int64_t offset_{0};
    };

} // namespace vix::sync::wal
