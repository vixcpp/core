#include <vix/sync/outbox/Outbox.hpp>

#include <utility>

namespace vix::sync::outbox
{

    Outbox::Outbox(Config cfg, std::shared_ptr<OutboxStore> store)
        : cfg_(std::move(cfg)), store_(std::move(store))
    {
    }

    std::string Outbox::enqueue(vix::sync::Operation op, std::int64_t now_ms)
    {
        if (cfg_.auto_generate_ids && op.id.empty())
        {
            op.id = make_id();
        }
        if (cfg_.auto_generate_idempotency_key && op.idempotency_key.empty())
        {
            op.idempotency_key = make_idempotency_key();
        }

        if (op.created_at_ms == 0)
            op.created_at_ms = now_ms;
        op.updated_at_ms = now_ms;

        if (op.next_retry_at_ms == 0)
            op.next_retry_at_ms = now_ms;
        if (op.status == vix::sync::OperationStatus::Pending)
        {
            // ok
        }

        store_->put(op);
        return op.id;
    }

    std::vector<vix::sync::Operation> Outbox::peek_ready(std::int64_t now_ms, std::size_t limit)
    {
        ListOptions opt;
        opt.limit = limit;
        opt.now_ms = now_ms;
        opt.only_ready = true;
        opt.include_inflight = false;
        return store_->list(opt);
    }

    bool Outbox::claim(const std::string &id, std::int64_t now_ms)
    {
        return store_->claim(id, cfg_.owner, now_ms);
    }

    bool Outbox::complete(const std::string &id, std::int64_t now_ms)
    {
        return store_->mark_done(id, now_ms);
    }

    bool Outbox::fail(const std::string &id, const std::string &error, std::int64_t now_ms)
    {
        auto cur = store_->get(id);
        if (!cur)
            return false;

        auto op = *cur;
        op.attempt += 1;
        op.updated_at_ms = now_ms;

        if (!cfg_.retry.can_retry(op.attempt))
        {
            // final fail (no more retries)
            return store_->mark_failed(id, error, now_ms, /*next_retry_at*/ now_ms);
        }

        const auto delay = cfg_.retry.compute_delay_ms(op.attempt);
        const auto next_at = now_ms + delay;
        return store_->mark_failed(id, error, now_ms, next_at);
    }

    std::string Outbox::make_id() { return "op_" + std::to_string(std::rand()); }
    std::string Outbox::make_idempotency_key() { return "idem_" + std::to_string(std::rand()); }

} // namespace vix::sync::outbox
