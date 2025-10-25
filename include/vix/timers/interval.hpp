#ifndef VIX_INTERVAL_HPP
#define VIX_INTERVAL_HPP

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>
#include <vix/executor/IExecutor.hpp>

namespace Vix::timers
{

    struct IntervalHandle
    {
        struct State
        {
            std::atomic<bool> stop{false};
        };

        std::shared_ptr<State> state;
        std::thread t;

        IntervalHandle() = default;

        // Non copiable
        IntervalHandle(const IntervalHandle &) = delete;
        IntervalHandle &operator=(const IntervalHandle &) = delete;

        // Déplaçable
        IntervalHandle(IntervalHandle &&other) noexcept
            : state(std::move(other.state)), t(std::move(other.t)) {}

        IntervalHandle &operator=(IntervalHandle &&other) noexcept
        {
            if (this != &other)
            {
                stopNow(); // arrêter/joindre l’ancien thread si besoin
                state = std::move(other.state);
                t = std::move(other.t);
            }
            return *this;
        }

        void stopNow()
        {
            if (state)
            {
                state->stop.store(true, std::memory_order_relaxed);
            }
            if (t.joinable())
                t.join();
        }

        ~IntervalHandle() { stopNow(); }
    };

    // Fabrique un périodique : poste fn() toutes les `period` via exec
    inline IntervalHandle interval(IExecutor &exec,
                                   std::chrono::milliseconds period,
                                   std::function<void()> fn,
                                   Vix::TaskOptions opt = {})
    {
        IntervalHandle h;
        h.state = std::make_shared<IntervalHandle::State>();
        std::weak_ptr<IntervalHandle::State> weak = h.state;

        h.t = std::thread([weak, &exec, period, fn = std::move(fn), opt]() mutable
                          {
        auto next = std::chrono::steady_clock::now() + period;
        for (;;) {
            auto st = weak.lock();
            if (!st) break; // handle détruit
            if (st->stop.load(std::memory_order_relaxed)) break;

            // On soumet la tâche (Fire-and-forget)
            (void)exec.post(fn, opt);

            std::this_thread::sleep_until(next);
            next += period;
        } });

        return h; // déplaçable, pas de copie
    }

} // namespace Vix::timers

#endif // VIX_INTERVAL_HPP
