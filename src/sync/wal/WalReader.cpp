#include <vix/sync/wal/WalReader.hpp>

#include <stdexcept>
#include <vector>

namespace vix::sync::wal
{

    static constexpr std::uint32_t kMagic = 0x56495857; // 'VIXW'
    static constexpr std::uint16_t kVersion = 1;

    WalReader::WalReader(std::filesystem::path p) : file_path_(std::move(p)) { open_(); }

    void WalReader::open_()
    {
        in_.open(file_path_, std::ios::binary);
    }

    void WalReader::seek(std::int64_t off)
    {
        offset_ = off;
        if (!in_)
            open_();
        if (!in_)
            return;
        in_.clear();
        in_.seekg(off, std::ios::beg);
    }

    std::optional<WalRecord> WalReader::next()
    {
        if (!in_)
            return std::nullopt;

        const auto start = static_cast<std::int64_t>(in_.tellg());
        if (in_.eof())
            return std::nullopt;

        std::uint32_t magic{};
        std::uint16_t version{};
        std::uint8_t type{};
        std::uint8_t reserved{};
        std::int64_t ts_ms{};
        std::uint32_t id_len{}, payload_len{}, error_len{};
        std::int64_t next_retry_at_ms{};

        in_.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (!in_)
            return std::nullopt;

        in_.read(reinterpret_cast<char *>(&version), sizeof(version));
        in_.read(reinterpret_cast<char *>(&type), sizeof(type));
        in_.read(reinterpret_cast<char *>(&reserved), sizeof(reserved));
        in_.read(reinterpret_cast<char *>(&ts_ms), sizeof(ts_ms));
        in_.read(reinterpret_cast<char *>(&id_len), sizeof(id_len));
        in_.read(reinterpret_cast<char *>(&payload_len), sizeof(payload_len));
        in_.read(reinterpret_cast<char *>(&error_len), sizeof(error_len));
        in_.read(reinterpret_cast<char *>(&next_retry_at_ms), sizeof(next_retry_at_ms));
        if (!in_)
            return std::nullopt;

        if (magic != kMagic || version != kVersion)
        {
            return std::nullopt;
        }

        WalRecord r;
        r.type = static_cast<RecordType>(type);
        r.ts_ms = ts_ms;
        r.next_retry_at_ms = next_retry_at_ms;

        if (id_len)
        {
            r.id.resize(id_len);
            in_.read(r.id.data(), id_len);
        }

        if (payload_len)
        {
            r.payload.resize(payload_len);
            in_.read(reinterpret_cast<char *>(r.payload.data()), payload_len);
        }

        if (error_len)
        {
            r.error.resize(error_len);
            in_.read(r.error.data(), error_len);
        }

        if (!in_)
            return std::nullopt;

        offset_ = start;
        return r;
    }

} // namespace vix::sync::wal
