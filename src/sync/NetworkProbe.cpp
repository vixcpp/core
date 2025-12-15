#include <vix/sync/NetworkProbe.hpp>

namespace vix::sync
{

    NetworkProbe::NetworkProbe(Config cfg, ProbeFn fn)
        : cfg_(cfg), probe_(std::move(fn))
    {
    }

    bool NetworkProbe::canProbe(std::int64_t now_ms) const noexcept
    {
        return (now_ms - last_probe_at_ms_) >= cfg_.min_interval_ms;
    }

    bool NetworkProbe::isOnline(std::int64_t now_ms) const
    {
        const auto age = now_ms - last_update_ms_;
        const auto ttl = last_online_ ? cfg_.online_ttl_ms : cfg_.offline_ttl_ms;

        if (age <= ttl)
        {
            return last_online_;
        }
        // cache expired -> caller should call refresh()
        return last_online_;
    }

    bool NetworkProbe::refresh(std::int64_t now_ms)
    {
        if (!probe_)
        {
            // No probe function -> assume online (or false). Choose conservative?
            last_online_ = true;
            last_update_ms_ = now_ms;
            return last_online_;
        }

        if (!canProbe(now_ms))
        {
            // Too soon: return cached
            return last_online_;
        }

        last_probe_at_ms_ = now_ms;
        const bool ok = probe_();
        last_online_ = ok;
        last_update_ms_ = now_ms;
        return last_online_;
    }

} // namespace vix::sync
