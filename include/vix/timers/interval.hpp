#ifndef VIX_INTERVAL_HPP
#define VIX_INTERVAL_HPP

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>
#include <vix/executor/IExecutor.hpp>

namespace vix::timers
{

    struct IntervalHandle
    {
        struct State
        {
            std::atomic<bool> stop{false};
        };

        std::shared_ptr<State> state;
        std::thread t;

        IntervalHandle()
            : state(nullptr), t() {}

        IntervalHandle(const IntervalHandle &) = delete;
        IntervalHandle &operator=(const IntervalHandle &) = delete;

        IntervalHandle(IntervalHandle &&other) noexcept
            : state(std::move(other.state)), t(std::move(other.t)) {}

        IntervalHandle &operator=(IntervalHandle &&other) noexcept
        {
            if (this != &other)
            {
                stopNow();
                state = std::move(other.state);
                t = std::move(other.t);
            }
            return *this;
        }

        void stopNow()
        {
            if (state)
                state->stop.store(true, std::memory_order_relaxed);
            if (t.joinable())
                t.join();
        }

        ~IntervalHandle() { stopNow(); }
    };

    inline IntervalHandle interval(vix::executor::IExecutor &exec,
                                   std::chrono::milliseconds period,
                                   std::function<void()> fn,
                                   vix::executor::TaskOptions opt = {})
    {
        IntervalHandle h;
        h.state = std::make_shared<IntervalHandle::State>();
        std::weak_ptr<IntervalHandle::State> weak = h.state;

        h.t = std::thread([weak, &exec, period, fn = std::move(fn), opt]() mutable
                          {
        auto next = std::chrono::steady_clock::now() + period;
        for (;;) {
            auto st = weak.lock();
            if (!st) break;
            if (st->stop.load(std::memory_order_relaxed)) break;

            (void)exec.post(fn, opt);
            std::this_thread::sleep_until(next);
            next += period;
        } });

        return h; // move-only
    }

} // namespace vix::timers

#endif // VIX_INTERVAL_HPP
