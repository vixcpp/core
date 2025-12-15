#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <vix/sync/Operation.hpp>
#include <vix/sync/NetworkProbe.hpp>
#include <vix/sync/outbox/Outbox.hpp>

namespace vix::sync::engine
{

    struct SendResult
    {
        bool ok{false};
        // If false => do not retry (permanent failure)
        bool retryable{true};
        // Optional error message
        std::string error;
    };

    class ISyncTransport
    {
    public:
        virtual ~ISyncTransport() = default;
        // Perform the actual send (HTTP/WS/P2P)
        virtual SendResult send(const vix::sync::Operation &op) = 0;
    };

    class SyncWorker
    {
    public:
        struct Config
        {
            std::size_t batch_limit{25};
            std::int64_t idle_sleep_ms{250}; // when nothing to do
            std::int64_t offline_sleep_ms{500};
        };

        SyncWorker(Config cfg,
                   std::shared_ptr<vix::sync::outbox::Outbox> outbox,
                   std::shared_ptr<vix::sync::NetworkProbe> probe,
                   std::shared_ptr<ISyncTransport> transport);
        // Process a single step (non-blocking style): one batch attempt
        // Returns number of operations processed (sent attempt).
        std::size_t tick(std::int64_t now_ms);

    private:
        bool should_send_(std::int64_t now_ms);
        std::size_t process_ready_(std::int64_t now_ms);

    private:
        Config cfg_;
        std::shared_ptr<vix::sync::outbox::Outbox> outbox_;
        std::shared_ptr<vix::sync::NetworkProbe> probe_;
        std::shared_ptr<ISyncTransport> transport_;
    };

} // namespace vix::sync::engine
