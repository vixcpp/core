/**
 *
 * @file interval.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
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

  /**
   * @brief RAII handle for a repeating interval task.
   *
   * Owns a small shared state used to stop the loop and a worker thread that
   * triggers scheduled executions through an executor.
   *
   * When destroyed, the handle stops the interval and joins the thread.
   */
  struct IntervalHandle
  {
    /**
     * @brief Shared stop state for the interval loop.
     *
     * The worker thread checks this flag periodically. Setting it to true
     * requests termination.
     */
    struct State
    {
      std::atomic<bool> stop{false};
    };

    /** @brief Shared stop state (kept alive while the interval is active). */
    std::shared_ptr<State> state;

    /** @brief Worker thread that wakes up at each period and posts the task. */
    std::thread t;

    /** @brief Construct an empty handle (no active interval). */
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

    /**
     * @brief Stop the interval immediately and join the worker thread.
     *
     * Safe to call multiple times. If no interval is active, this is a no-op.
     */
    void stopNow()
    {
      if (state)
        state->stop.store(true, std::memory_order_relaxed);
      if (t.joinable())
        t.join();
    }

    /** @brief Destructor: stops the interval and joins the thread (RAII). */
    ~IntervalHandle() { stopNow(); }
  };

  /**
   * @brief Schedule a repeating task at a fixed interval.
   *
   * Creates a background thread that, every @p period, posts @p fn to the given
   * executor using @p opt.
   *
   * The returned @ref IntervalHandle can be used to stop the interval via
   * @ref IntervalHandle::stopNow. Destroying the handle also stops it.
   *
   * @param exec Executor used to post the task.
   * @param period Interval between executions.
   * @param fn Callback to execute (posted to the executor).
   * @param opt Task options forwarded to the executor.
   * @return IntervalHandle RAII handle controlling the interval lifetime.
   */
  inline IntervalHandle interval(
      vix::executor::IExecutor &exec,
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

    return h;
  }

} // namespace vix::timers

#endif // VIX_INTERVAL_HPP
