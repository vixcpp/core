#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>

#include <vix/sync/wal/WalRecord.hpp>

namespace vix::sync::wal
{

    class WalWriter
    {
    public:
        struct Config
        {
            std::filesystem::path file_path;
            bool fsync_on_write{false}; // slower, safer
        };

        explicit WalWriter(Config cfg);
        ~WalWriter();
        std::int64_t append(const WalRecord &rec);
        void flush();

    private:
        void open_();
        std::int64_t tell_();

    private:
        Config cfg_;
        std::ofstream out_;
    };

} // namespace vix::sync::wal
