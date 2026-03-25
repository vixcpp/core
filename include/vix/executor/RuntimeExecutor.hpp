/**
 *
 * @file RuntimeExecutor.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP
#define VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include <vix/executor/Metrics.hpp>
#include <vix/executor/TaskOptions.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/runtime/Task.hpp>

namespace vix::executor
{

  /**
   * @brief Executor adapter built on top of vix::runtime::Runtime.
   *
   * This class exposes an executor-style API while delegating actual scheduling
   * and execution to the Vix runtime.
   *
   * Design goals:
   * - idempotent start/stop
   * - safe destruction
   * - no task captures of the executor raw pointer
   * - lightweight metrics for higher-level systems
   */
  class RuntimeExecutor final
  {
  public:
    /**
     * @brief Construct a RuntimeExecutor with a runtime configuration.
     *
     * @param config Runtime configuration used to create the underlying runtime.
     */
    explicit RuntimeExecutor(
        const vix::runtime::RuntimeConfig &config = vix::runtime::RuntimeConfig{})
        : runtime_(std::make_unique<vix::runtime::Runtime>(config)),
          state_(std::make_shared<SharedState>()),
          started_(false)
    {
    }

    /**
     * @brief Construct a RuntimeExecutor with an explicit worker count.
     *
     * This is a convenience constructor for higher-level code such as App or
     * HTTPServer. It still uses the runtime internally.
     *
     * @param workers Number of runtime workers to use.
     */
    explicit RuntimeExecutor(std::uint32_t workers)
        : RuntimeExecutor(make_config_from_workers(workers))
    {
    }

    /**
     * @brief Construct a RuntimeExecutor from an existing runtime instance.
     *
     * @param runtime Pre-built runtime instance.
     *
     * @throw std::invalid_argument If runtime is null.
     */
    explicit RuntimeExecutor(std::unique_ptr<vix::runtime::Runtime> runtime)
        : runtime_(std::move(runtime)),
          state_(std::make_shared<SharedState>()),
          started_(false)
    {
      if (!runtime_)
      {
        throw std::invalid_argument("RuntimeExecutor requires a valid runtime");
      }
    }

    RuntimeExecutor(const RuntimeExecutor &) = delete;
    RuntimeExecutor &operator=(const RuntimeExecutor &) = delete;

    RuntimeExecutor(RuntimeExecutor &&) = delete;
    RuntimeExecutor &operator=(RuntimeExecutor &&) = delete;

    /**
     * @brief Destroy the executor and stop the runtime safely.
     */
    ~RuntimeExecutor() noexcept
    {
      try
      {
        stop();
      }
      catch (...)
      {
      }
    }

    /**
     * @brief Start the underlying runtime once.
     *
     * Safe to call multiple times.
     */
    void start()
    {
      std::lock_guard<std::mutex> lock(lifecycleMutex_);

      if (started_.load(std::memory_order_acquire))
      {
        return;
      }

      runtime_->start();
      started_.store(true, std::memory_order_release);
      state_->accepting.store(true, std::memory_order_release);
    }

    /**
     * @brief Stop the underlying runtime once.
     *
     * Safe to call multiple times.
     * This is a blocking stop because vix::runtime::Runtime::stop()
     * blocks until the scheduler workers are joined.
     */
    void stop()
    {
      std::lock_guard<std::mutex> lock(lifecycleMutex_);

      if (!started_.load(std::memory_order_acquire))
      {
        state_->accepting.store(false, std::memory_order_release);
        return;
      }

      state_->accepting.store(false, std::memory_order_release);
      runtime_->stop();
      started_.store(false, std::memory_order_release);
    }

    /**
     * @brief Stop the executor after waiting for current work to drain.
     *
     * This first waits for queued and active work to reach zero, then stops
     * the runtime.
     *
     * Use this when graceful shutdown is preferred over immediate stop.
     */
    void stop_and_wait()
    {
      wait_idle();
      stop();
    }

    /**
     * @brief Return whether the runtime has been started.
     *
     * @return true if the executor is started.
     */
    [[nodiscard]] bool started() const noexcept
    {
      return started_.load(std::memory_order_acquire);
    }

    /**
     * @brief Return whether the underlying runtime is currently running.
     *
     * @return true if the runtime scheduler is running.
     */
    [[nodiscard]] bool running() const noexcept
    {
      return runtime_->running();
    }

    /**
     * @brief Submit a pre-built runtime task directly.
     *
     * @param task Runtime task to submit.
     *
     * @return true if the task was accepted, false otherwise.
     */
    [[nodiscard]] bool submit(vix::runtime::Task task)
    {
      if (!state_->accepting.load(std::memory_order_acquire))
      {
        return false;
      }

      return runtime_->submit(std::move(task));
    }

    /**
     * @brief Submit a runtime task function directly.
     *
     * This overload is useful for code paths that already use the runtime task
     * contract and want to return TaskResult values such as complete or yield.
     *
     * @param fn Runtime task function.
     * @param affinity Optional worker affinity hint.
     *
     * @return true if the task was accepted, false otherwise.
     */
    [[nodiscard]] bool submit(vix::runtime::TaskFn fn,
                              std::uint32_t affinity = 0)
    {
      if (!state_->accepting.load(std::memory_order_acquire))
      {
        return false;
      }

      return runtime_->submit(std::move(fn), affinity);
    }

    /**
     * @brief Submit a void task to the runtime.
     *
     * This adapts a void callable to the runtime task contract.
     *
     * Notes:
     * - metrics are updated through shared state
     * - the task does not capture the RuntimeExecutor raw pointer
     * - if the executor is no longer accepting work, submission is rejected
     *
     * @param fn Task function to execute.
     * @param opt Task execution options.
     *
     * @return true if the task was accepted, false otherwise.
     */
    [[nodiscard]] bool post(std::function<void()> fn,
                            TaskOptions opt = {})
    {
      if (!fn)
      {
        return false;
      }

      if (!state_->accepting.load(std::memory_order_acquire))
      {
        return false;
      }

      const auto shared = state_;

      return runtime_->submit(
          [shared,
           task = std::move(fn),
           options = opt]() mutable -> vix::runtime::TaskResult
          {
            shared->active.fetch_add(1, std::memory_order_relaxed);

            const auto startTime = std::chrono::steady_clock::now();

            try
            {
              task();

              shared->active.fetch_sub(1, std::memory_order_relaxed);

              if (options.timeout.count() > 0)
              {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime);

                if (elapsed > options.timeout)
                {
                  shared->timed_out.fetch_add(1, std::memory_order_relaxed);
                }
              }

              return vix::runtime::TaskResult::complete;
            }
            catch (...)
            {
              shared->active.fetch_sub(1, std::memory_order_relaxed);

              if (options.timeout.count() > 0)
              {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime);

                if (elapsed > options.timeout)
                {
                  shared->timed_out.fetch_add(1, std::memory_order_relaxed);
                }
              }

              return vix::runtime::TaskResult::failed;
            }
          });
    }

    /**
     * @brief Return executor metrics derived from the runtime state.
     *
     * @return Metrics snapshot of pending, active and timed out tasks.
     */
    [[nodiscard]] vix::executor::Metrics metrics() const
    {
      vix::executor::Metrics m;
      m.pending = static_cast<std::uint64_t>(runtime_->size());
      m.active = state_->active.load(std::memory_order_relaxed);
      m.timed_out = state_->timed_out.load(std::memory_order_relaxed);
      return m;
    }

    /**
     * @brief Block until the runtime has no pending or active tasks.
     *
     * This does not stop the runtime.
     */
    void wait_idle() const
    {
      for (;;)
      {
        const auto m = metrics();
        if (m.pending == 0 && m.active == 0)
        {
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    /**
     * @brief Return the underlying runtime.
     *
     * @return vix::runtime::Runtime& Mutable runtime reference.
     */
    [[nodiscard]] vix::runtime::Runtime &runtime() noexcept
    {
      return *runtime_;
    }

    /**
     * @brief Return the underlying runtime.
     *
     * @return const vix::runtime::Runtime& Const runtime reference.
     */
    [[nodiscard]] const vix::runtime::Runtime &runtime() const noexcept
    {
      return *runtime_;
    }

  private:
    /**
     * @brief Shared state captured by posted tasks.
     *
     * This avoids capturing the RuntimeExecutor raw pointer inside runtime tasks,
     * which could otherwise become dangling during shutdown or destruction.
     */
    struct SharedState
    {
      std::atomic<std::uint64_t> active{0};
      std::atomic<std::uint64_t> timed_out{0};
      std::atomic<bool> accepting{false};
    };

    /**
     * @brief Build a runtime configuration from a worker count.
     *
     * @param workers Requested worker count.
     *
     * @return vix::runtime::RuntimeConfig Normalized runtime configuration.
     */
    [[nodiscard]] static vix::runtime::RuntimeConfig make_config_from_workers(std::uint32_t workers)
    {
      const std::uint32_t normalized_workers = std::max<std::uint32_t>(1u, workers);

      return vix::runtime::RuntimeConfig{
          normalized_workers,
          vix::runtime::BudgetConfig{16}};
    }

  private:
    /** @brief Owned runtime facade. */
    std::unique_ptr<vix::runtime::Runtime> runtime_;

    /** @brief Shared state used by submitted tasks. */
    std::shared_ptr<SharedState> state_;

    /** @brief Mutex serializing lifecycle transitions. */
    mutable std::mutex lifecycleMutex_;

    /** @brief Started flag for idempotent start/stop. */
    std::atomic<bool> started_;
  };

} // namespace vix::executor

#endif // VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP
