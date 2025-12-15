#include <vix/sync/engine/SyncEngine.hpp>

#include <chrono>

namespace vix::sync::engine
{

    static std::int64_t now_ms()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    SyncEngine::SyncEngine(Config cfg,
                           std::shared_ptr<vix::sync::outbox::Outbox> outbox,
                           std::shared_ptr<vix::sync::NetworkProbe> probe,
                           std::shared_ptr<ISyncTransport> transport)
        : cfg_(cfg),
          outbox_(std::move(outbox)),
          probe_(std::move(probe)),
          transport_(std::move(transport))
    {
        workers_.reserve(cfg_.worker_count);
        for (std::size_t i = 0; i < cfg_.worker_count; ++i)
        {
            SyncWorker::Config wc;
            wc.batch_limit = cfg_.batch_limit;
            wc.idle_sleep_ms = cfg_.idle_sleep_ms;
            wc.offline_sleep_ms = cfg_.offline_sleep_ms;

            workers_.push_back(std::make_unique<SyncWorker>(wc, outbox_, probe_, transport_));
        }
    }

    SyncEngine::~SyncEngine()
    {
        stop();
    }

    std::size_t SyncEngine::tick(std::int64_t t_ms)
    {
        std::size_t total = 0;
        for (auto &w : workers_)
        {
            total += w->tick(t_ms);
        }
        return total;
    }

    void SyncEngine::start()
    {
        if (running_.exchange(true))
            return;
        thread_ = std::thread([this]
                              { run_loop_(); });
    }

    void SyncEngine::stop()
    {
        if (!running_.exchange(false))
            return;
        if (thread_.joinable())
            thread_.join();
    }

    void SyncEngine::run_loop_()
    {
        while (running_.load())
        {
            const auto t = now_ms();
            const auto processed = tick(t);

            const auto sleep_ms = (processed == 0) ? cfg_.idle_sleep_ms : 0;
            if (sleep_ms > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

} // namespace vix::sync::engine
