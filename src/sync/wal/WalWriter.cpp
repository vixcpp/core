#include <vix/sync/wal/WalWriter.hpp>

#include <stdexcept>

namespace vix::sync::wal
{

    static constexpr std::uint32_t kMagic = 0x56495857; // 'VIXW'
    static constexpr std::uint16_t kVersion = 1;

    WalWriter::WalWriter(Config cfg) : cfg_(std::move(cfg)) { open_(); }
    WalWriter::~WalWriter() { flush(); }

    void WalWriter::open_()
    {
        std::filesystem::create_directories(cfg_.file_path.parent_path());
        out_.open(cfg_.file_path, std::ios::binary | std::ios::app);
        if (!out_)
            throw std::runtime_error("WalWriter: cannot open file");
    }

    std::int64_t WalWriter::tell_()
    {
        return static_cast<std::int64_t>(out_.tellp());
    }

    std::int64_t WalWriter::append(const WalRecord &r)
    {
        if (!out_)
            open_();

        const auto offset = tell_();

        const std::uint32_t id_len = static_cast<std::uint32_t>(r.id.size());
        const std::uint32_t payload_len = static_cast<std::uint32_t>(r.payload.size());
        const std::uint32_t error_len = static_cast<std::uint32_t>(r.error.size());

        // header
        out_.write(reinterpret_cast<const char *>(&kMagic), sizeof(kMagic));
        out_.write(reinterpret_cast<const char *>(&kVersion), sizeof(kVersion));

        const std::uint8_t type = static_cast<std::uint8_t>(r.type);
        out_.write(reinterpret_cast<const char *>(&type), sizeof(type));

        const std::uint8_t reserved = 0;
        out_.write(reinterpret_cast<const char *>(&reserved), sizeof(reserved));

        out_.write(reinterpret_cast<const char *>(&r.ts_ms), sizeof(r.ts_ms));
        out_.write(reinterpret_cast<const char *>(&id_len), sizeof(id_len));
        out_.write(reinterpret_cast<const char *>(&payload_len), sizeof(payload_len));
        out_.write(reinterpret_cast<const char *>(&error_len), sizeof(error_len));
        out_.write(reinterpret_cast<const char *>(&r.next_retry_at_ms), sizeof(r.next_retry_at_ms));

        // body
        if (id_len)
            out_.write(r.id.data(), id_len);
        if (payload_len)
            out_.write(reinterpret_cast<const char *>(r.payload.data()), payload_len);
        if (error_len)
            out_.write(r.error.data(), error_len);

        flush();
        return offset;
    }

    void WalWriter::flush()
    {
        if (!out_)
            return;
        out_.flush();
    }

} // namespace vix::sync::wal
