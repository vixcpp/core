#include <vix/sync/wal/Wal.hpp>
#include <vix/sync/wal/WalWriter.hpp>
#include <vix/sync/wal/WalReader.hpp>

namespace vix::sync::wal
{

    Wal::Wal(Config cfg) : cfg_(std::move(cfg)) {}

    std::int64_t Wal::append(const WalRecord &rec)
    {
        WalWriter w({cfg_.file_path, cfg_.fsync_on_write});
        return w.append(rec);
    }

    std::int64_t Wal::replay(std::int64_t from_offset,
                             const std::function<void(const WalRecord &)> &on_record)
    {
        WalReader r(cfg_.file_path);
        r.seek(from_offset);

        std::int64_t last = -1;
        while (true)
        {
            auto rec = r.next();
            if (!rec)
                break;
            on_record(*rec);
            last = r.current_offset();
        }
        return last;
    }

} // namespace vix::sync::wal
