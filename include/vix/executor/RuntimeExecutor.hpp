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
   * This class provides an executor-style API for components that still expect
   * an executor object, while delegating actual scheduling and execution to the
   * new runtime engine.
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
          active_(0),
          timed_out_(0),
          started_(false)
    {
    }

    /**
     * @brief Construct a RuntimeExecutor with an explicit worker count.
     *
     * This is a convenience constructor for higher-level code such as App or
     * HTTPServer. It still uses the runtime internally and does not depend on
     * any threadpool executor.
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
          active_(0),
          timed_out_(0),
          started_(false)
    {
      if (!runtime_)
      {
        throw std::invalid_argument("RuntimeExecutor requires a valid runtime");
      }
    }

    /**
     * @brief Destroy the executor and stop the runtime if needed.
     */
    ~RuntimeExecutor()
    {
      stop();
    }

    RuntimeExecutor(const RuntimeExecutor &) = delete;
    RuntimeExecutor &operator=(const RuntimeExecutor &) = delete;

    RuntimeExecutor(RuntimeExecutor &&) = delete;
    RuntimeExecutor &operator=(RuntimeExecutor &&) = delete;

    /**
     * @brief Start the underlying runtime once.
     */
    void start()
    {
      bool expected = false;
      if (started_.compare_exchange_strong(expected,
                                           true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
      {
        runtime_->start();
      }
    }

    /**
     * @brief Stop the underlying runtime once.
     */
    void stop()
    {
      bool expected = true;
      if (started_.compare_exchange_strong(expected,
                                           false,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
      {
        runtime_->stop();
      }
    }

    /**
     * @brief Submit a runtime task directly.
     *
     * This forwards a fully constructed task to the underlying runtime.
     *
     * @param task Runtime task to submit.
     *
     * @return true if the task was accepted, false otherwise.
     */
    [[nodiscard]] bool submit(vix::runtime::Task task)
    {
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
      return runtime_->submit(std::move(fn), affinity);
    }

    /**
     * @brief Submit a void task to the runtime.
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

      (void)opt;

      return runtime_->submit(
          [this, task = std::move(fn)]() mutable -> vix::runtime::TaskResult
          {
            active_.fetch_add(1, std::memory_order_relaxed);

            try
            {
              task();
              active_.fetch_sub(1, std::memory_order_relaxed);
              return vix::runtime::TaskResult::complete;
            }
            catch (...)
            {
              active_.fetch_sub(1, std::memory_order_relaxed);
              return vix::runtime::TaskResult::failed;
            }
          });
    }

    /**
     * @brief Return executor metrics derived from the runtime state.
     *
     * @return Metrics Snapshot of pending, active and timed out tasks.
     */
    [[nodiscard]] vix::executor::Metrics metrics() const
    {
      vix::executor::Metrics m;
      m.pending = static_cast<std::uint64_t>(runtime_->size());
      m.active = active_.load(std::memory_order_relaxed);
      m.timed_out = timed_out_.load(std::memory_order_relaxed);
      return m;
    }

    /**
     * @brief Block until the runtime has no pending or active tasks.
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
     * @brief Build a runtime configuration from a worker count.
     *
     * @param workers Requested worker count.
     *
     * @return vix::runtime::RuntimeConfig Normalized runtime configuration.
     */
    static vix::runtime::RuntimeConfig make_config_from_workers(std::uint32_t workers)
    {
      const std::uint32_t normalized_workers = std::max<std::uint32_t>(1u, workers);

      return vix::runtime::RuntimeConfig{
          normalized_workers,
          vix::runtime::BudgetConfig{16}};
    }

  private:
    std::unique_ptr<vix::runtime::Runtime> runtime_;
    std::atomic<std::uint64_t> active_;
    std::atomic<std::uint64_t> timed_out_;
    std::atomic<bool> started_;
  };

} // namespace vix::executor

#endif // VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP
