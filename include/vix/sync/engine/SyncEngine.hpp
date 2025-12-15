#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <vix/sync/NetworkProbe.hpp>
#include <vix/sync/outbox/Outbox.hpp>
#include <vix/sync/engine/SyncWorker.hpp>

namespace vix::sync::engine
{

    class SyncEngine
    {
    public:
        struct Config
        {
            std::size_t worker_count{1};
            // Engine loop
            std::int64_t idle_sleep_ms{250};
            std::int64_t offline_sleep_ms{500};
            // Worker
            std::size_t batch_limit{25};
        };

        SyncEngine(Config cfg,
                   std::shared_ptr<vix::sync::outbox::Outbox> outbox,
                   std::shared_ptr<vix::sync::NetworkProbe> probe,
                   std::shared_ptr<ISyncTransport> transport);

        ~SyncEngine();
        // Tick-based (manual driving)
        std::size_t tick(std::int64_t now_ms);
        // Threaded mode
        void start();
        void stop();
        bool running() const noexcept { return running_.load(); }

    private:
        void run_loop_();

    private:
        Config cfg_;
        std::shared_ptr<vix::sync::outbox::Outbox> outbox_;
        std::shared_ptr<vix::sync::NetworkProbe> probe_;
        std::shared_ptr<ISyncTransport> transport_;
        std::vector<std::unique_ptr<SyncWorker>> workers_;
        std::atomic<bool> running_{false};
        std::thread thread_;
    };

} // namespace vix::sync::engine
