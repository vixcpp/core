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
   * - start and stop execution
   * - submit lightweight tasks
   * - generate unique task ids
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
          nextTaskId_(1),
          started_(false),
          submittedTasks_(0),
          rejectedTasks_(0)
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
     *
     * Idempotent.
     */
    void start()
    {
      bool expected = false;
      if (!started_.compare_exchange_strong(expected,
                                            true,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
      {
        return;
      }

      scheduler_.start();
    }

    /**
     * @brief Stop the runtime scheduler.
     *
     * Idempotent.
     */
    void stop()
    {
      bool expected = true;
      if (!started_.compare_exchange_strong(expected,
                                            false,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
      {
        return;
      }

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
     * @brief Return whether the runtime has been started.
     *
     * @return true if start() has been called successfully.
     */
    [[nodiscard]] bool started() const noexcept
    {
      return started_.load(std::memory_order_acquire);
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
      if (!started())
      {
        rejectedTasks_.fetch_add(1, std::memory_order_relaxed);
        return false;
      }

      if (!task.schedulable())
      {
        rejectedTasks_.fetch_add(1, std::memory_order_relaxed);
        return false;
      }

      if (task.id == 0)
      {
        task.id = next_task_id();
      }

      const bool accepted = scheduler_.submit(std::move(task));

      if (accepted)
      {
        submittedTasks_.fetch_add(1, std::memory_order_relaxed);
      }
      else
      {
        rejectedTasks_.fetch_add(1, std::memory_order_relaxed);
      }

      return accepted;
    }

    /**
     * @brief Submit a callable as a runtime task.
     *
     * @param fn Task callable.
     * @param affinity Optional worker affinity hint.
     * @return true if the task was accepted.
     */
    [[nodiscard]] bool submit(TaskFn fn, std::uint32_t affinity = 0)
    {
      if (!fn)
      {
        rejectedTasks_.fetch_add(1, std::memory_order_relaxed);
        return false;
      }

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
     * @brief Return runtime configuration.
     */
    [[nodiscard]] const RuntimeConfig &config() const noexcept
    {
      return config_;
    }

    /**
     * @brief Return number of submitted tasks.
     */
    [[nodiscard]] std::uint64_t submitted_tasks() const noexcept
    {
      return submittedTasks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Return number of rejected tasks.
     */
    [[nodiscard]] std::uint64_t rejected_tasks() const noexcept
    {
      return rejectedTasks_.load(std::memory_order_relaxed);
    }

  private:
    /**
     * @brief Normalize runtime configuration.
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

      config.budget.quantum =
          BudgetConfig::normalize_quantum(config.budget.quantum);

      return config;
    }

  private:
    /** @brief Normalized runtime configuration. */
    RuntimeConfig config_;

    /** @brief Internal scheduler used by the runtime. */
    Scheduler scheduler_;

    /** @brief Monotonic task id generator. */
    std::atomic<TaskId> nextTaskId_;

    /** @brief Started flag. */
    std::atomic<bool> started_;

    /** @brief Metrics. */
    std::atomic<std::uint64_t> submittedTasks_;
    std::atomic<std::uint64_t> rejectedTasks_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_RUNTIME_HPP
