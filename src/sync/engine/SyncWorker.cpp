#include <vix/sync/engine/SyncWorker.hpp>

namespace vix::sync::engine
{

    SyncWorker::SyncWorker(Config cfg,
                           std::shared_ptr<vix::sync::outbox::Outbox> outbox,
                           std::shared_ptr<vix::sync::NetworkProbe> probe,
                           std::shared_ptr<ISyncTransport> transport)
        : cfg_(cfg),
          outbox_(std::move(outbox)),
          probe_(std::move(probe)),
          transport_(std::move(transport))
    {
    }

    bool SyncWorker::should_send_(std::int64_t now_ms)
    {
        // Refresh if cache likely stale (probe controls throttling)
        const bool online = probe_ ? probe_->refresh(now_ms) : true;
        return online;
    }

    std::size_t SyncWorker::process_ready_(std::int64_t now_ms)
    {
        auto ops = outbox_->peek_ready(now_ms, cfg_.batch_limit);
        if (ops.empty())
            return 0;

        std::size_t processed = 0;

        for (const auto &op : ops)
        {
            // Claim to avoid double send
            if (!outbox_->claim(op.id, now_ms))
            {
                continue;
            }

            SendResult r;
            if (transport_)
            {
                r = transport_->send(op);
            }
            else
            {
                r.ok = false;
                r.retryable = true;
                r.error = "No transport configured";
            }

            if (r.ok)
            {
                outbox_->complete(op.id, now_ms);
            }
            else
            {
                outbox_->fail(op.id, r.error.empty() ? "send failed" : r.error, now_ms);
            }

            ++processed;
        }

        return processed;
    }

    std::size_t SyncWorker::tick(std::int64_t now_ms)
    {
        if (!outbox_)
            return 0;

        if (!should_send_(now_ms))
        {
            return 0;
        }

        return process_ready_(now_ms);
    }

} // namespace vix::sync::engine
