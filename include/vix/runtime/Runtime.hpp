/**
 *
 * @file Runtime.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the
 * License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_RUNTIME_RUNTIME_HPP
#define VIX_RUNTIME_RUNTIME_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <utility>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Scheduler.hpp>
#include <vix/runtime/Task.hpp>

namespace vix::runtime
{

  /**
   * @brief Configuration for the Vix lightweight runtime.
   */
  struct RuntimeConfig
  {
    /**
     * @brief Number of worker threads used by the runtime.
     *
     * A value of 0 means "auto-detect from hardware".
     */
    std::uint32_t workerCount;

    /**
     * @brief Per-task scheduling budget configuration.
     */
    BudgetConfig budget;

    /**
     * @brief Construct a runtime configuration.
     *
     * @param workers Number of workers. 0 means auto-detect.
     * @param budgetConfig Per-task budget configuration.
     */
    explicit RuntimeConfig(std::uint32_t workers = 0,
                           BudgetConfig budgetConfig = BudgetConfig{}) noexcept
        : workerCount(workers),
          budget(budgetConfig)
    {
    }
  };

  /**
   * @brief Public runtime facade for lightweight task scheduling.
   *
   * Runtime owns one scheduler and provides a simple API to:
   * - start and stop execution,
   * - submit lightweight tasks,
   * - generate unique task ids.
   */
  class Runtime
  {
  public:
    /**
     * @brief Construct a runtime from configuration.
     *
     * @param config Runtime configuration.
     */
    explicit Runtime(const RuntimeConfig &config = RuntimeConfig{})
        : config_(normalize_config(config)),
          scheduler_(config_.workerCount, config_.budget),
          nextTaskId_(1)
    {
    }

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    Runtime(Runtime &&) = delete;
    Runtime &operator=(Runtime &&) = delete;

    /**
     * @brief Stop the runtime on destruction.
     */
    ~Runtime()
    {
      stop();
    }

    /**
     * @brief Start the runtime scheduler.
     */
    void start()
    {
      scheduler_.start();
    }

    /**
     * @brief Stop the runtime scheduler.
     */
    void stop()
    {
      scheduler_.stop();
    }

    /**
     * @brief Return whether the runtime is running.
     *
     * @return true if the scheduler is running.
     */
    [[nodiscard]] bool running() const noexcept
    {
      return scheduler_.running();
    }

    /**
     * @brief Return the configured runtime worker count.
     *
     * @return Number of workers used by the runtime.
     */
    [[nodiscard]] std::uint32_t worker_count() const noexcept
    {
      return scheduler_.worker_count();
    }

    /**
     * @brief Return the number of queued tasks across all workers.
     *
     * @return Total queued task count.
     */
    [[nodiscard]] std::size_t size() const
    {
      return scheduler_.size();
    }

    /**
     * @brief Return whether all worker queues are currently empty.
     *
     * @return true if no queued task exists.
     */
    [[nodiscard]] bool empty() const
    {
      return scheduler_.empty();
    }

    /**
     * @brief Generate the next unique runtime task id.
     *
     * @return Unique task id.
     */
    [[nodiscard]] TaskId next_task_id() noexcept
    {
      return nextTaskId_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Submit a pre-built task to the runtime.
     *
     * @param task Task to submit.
     * @return true if the task was accepted.
     */
    [[nodiscard]] bool submit(Task task)
    {
      if (!task.valid())
      {
        return false;
      }

      if (task.id == 0)
      {
        task.id = next_task_id();
      }

      return scheduler_.submit(std::move(task));
    }

    /**
     * @brief Submit a callable as a runtime task.
     *
     * The callable must match TaskFn semantics and return TaskResult.
     *
     * @param fn Task callable.
     * @param affinity Optional worker affinity hint.
     * @return true if the task was accepted.
     */
    [[nodiscard]] bool submit(TaskFn fn, std::uint32_t affinity = 0)
    {
      return submit(Task(next_task_id(), std::move(fn), affinity));
    }

    /**
     * @brief Build a task without submitting it yet.
     *
     * @param fn Task callable.
     * @param affinity Optional worker affinity hint.
     * @return A ready-to-submit task.
     */
    [[nodiscard]] Task make_task(TaskFn fn, std::uint32_t affinity = 0)
    {
      return Task(next_task_id(), std::move(fn), affinity);
    }

    /**
     * @brief Access the runtime configuration.
     *
     * @return Immutable runtime configuration.
     */
    [[nodiscard]] const RuntimeConfig &config() const noexcept
    {
      return config_;
    }

  private:
    /**
     * @brief Normalize runtime configuration.
     *
     * If worker count is zero, it is resolved from hardware concurrency.
     *
     * @param config Input configuration.
     * @return Normalized configuration.
     */
    [[nodiscard]] static RuntimeConfig normalize_config(RuntimeConfig config) noexcept
    {
      if (config.workerCount == 0)
      {
        const unsigned int hc = std::thread::hardware_concurrency();
        config.workerCount = (hc == 0u) ? 1u : static_cast<std::uint32_t>(hc);
      }

      if (config.workerCount == 0)
      {
        config.workerCount = 1;
      }

      return config;
    }

  private:
    /** @brief Normalized runtime configuration. */
    RuntimeConfig config_;

    /** @brief Internal scheduler used by the runtime. */
    Scheduler scheduler_;

    /** @brief Monotonic task id generator. */
    std::atomic<TaskId> nextTaskId_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_RUNTIME_HPP
