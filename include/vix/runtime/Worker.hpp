/**
 *
 * @file Worker.hpp
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
#ifndef VIX_RUNTIME_WORKER_HPP
#define VIX_RUNTIME_WORKER_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <utility>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/RunQueue.hpp>
#include <vix/runtime/Task.hpp>

namespace vix::runtime
{

  /**
   * @brief Callback used by a worker to try stealing work from the scheduler.
   *
   * The callback receives the current worker id and returns one stolen task
   * if available.
   */
  using StealFn = std::function<std::optional<Task>(std::uint32_t)>;

  /**
   * @brief Runtime worker executing lightweight tasks on one OS thread.
   *
   * Each worker owns a local run queue and repeatedly:
   * - pops a local task,
   * - executes it,
   * - reschedules yielded tasks,
   * - or tries to steal work if idle.
   *
   * Lifecycle:
   * - start() launches the worker thread once
   * - stop() requests shutdown
   * - join() waits for thread completion
   * - destruction performs stop() + join()
   *
   * Shutdown behavior:
   * - once stopping is requested, the worker loop exits as soon as possible
   * - yielded tasks are no longer resubmitted after shutdown begins
   *
   * Thread-safety:
   * - submit() is safe as long as RunQueue is thread-safe
   * - start/stop state is coordinated through atomics
   */
  class Worker
  {
  public:
    /**
     * @brief Construct a worker.
     *
     * @param workerId Unique worker identifier.
     * @param budgetConfig Scheduling budget configuration.
     */
    explicit Worker(
        std::uint32_t workerId,
        const BudgetConfig &budgetConfig = BudgetConfig{}) noexcept
        : id_(workerId),
          queue_(),
          running_(false),
          budgetConfig_(budgetConfig),
          stealFn_(),
          thread_()
    {
    }

    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;

    Worker(Worker &&) = delete;
    Worker &operator=(Worker &&) = delete;

    /**
     * @brief Stop and join the worker thread on destruction.
     */
    ~Worker()
    {
      stop();
      join();
    }

    /**
     * @brief Return the worker identifier.
     *
     * @return Worker id.
     */
    [[nodiscard]] std::uint32_t id() const noexcept
    {
      return id_;
    }

    /**
     * @brief Set the external steal callback.
     *
     * @param fn Callback used when the worker has no local task.
     */
    void set_steal_callback(StealFn fn)
    {
      stealFn_ = std::move(fn);
    }

    /**
     * @brief Start the worker thread.
     *
     * Has no effect if the worker is already running.
     *
     * If a previous thread object is still joinable, it is joined before
     * launching a new one. This prevents std::terminate caused by assigning
     * a new std::thread over a joinable thread.
     */
    void start()
    {
      bool expected = false;
      if (!running_.compare_exchange_strong(
              expected,
              true,
              std::memory_order_acq_rel,
              std::memory_order_acquire))
      {
        return;
      }

      if (thread_.joinable())
      {
        thread_.join();
      }

      thread_ = std::thread([this]()
                            { run_loop(); });
    }

    /**
     * @brief Request the worker to stop.
     *
     * The loop exits naturally as soon as possible.
     */
    void stop() noexcept
    {
      running_.store(false, std::memory_order_release);
    }

    /**
     * @brief Join the worker thread if joinable.
     */
    void join() noexcept
    {
      if (thread_.joinable())
      {
        thread_.join();
      }
    }

    /**
     * @brief Enqueue a task into the local run queue.
     *
     * New tasks are rejected once the worker is stopping.
     *
     * @param task Task to enqueue.
     * @return true if enqueued successfully, false otherwise.
     */
    [[nodiscard]] bool submit(Task task)
    {
      if (!running())
      {
        return false;
      }

      return queue_.push(std::move(task));
    }

    /**
     * @brief Return whether the worker is currently running.
     *
     * @return true if the worker loop is active.
     */
    [[nodiscard]] bool running() const noexcept
    {
      return running_.load(std::memory_order_acquire);
    }

    /**
     * @brief Return whether the local queue is empty.
     *
     * @return true if no local task is queued.
     */
    [[nodiscard]] bool empty() const
    {
      return queue_.empty();
    }

    /**
     * @brief Return the approximate local queue size.
     *
     * @return Number of tasks in the local queue.
     */
    [[nodiscard]] std::size_t size() const
    {
      return queue_.size();
    }

    /**
     * @brief Try to steal one task from this worker.
     *
     * Used by the scheduler when another worker is idle.
     *
     * @return A stolen task if available.
     */
    [[nodiscard]] std::optional<Task> try_steal()
    {
      return queue_.try_steal();
    }

  private:
    /**
     * @brief Main worker loop.
     *
     * The worker prefers local tasks first, then attempts work stealing.
     * If no work is available, it waits briefly to avoid hot spinning.
     */
    void run_loop()
    {
      while (running())
      {
        std::optional<Task> task = queue_.try_pop();

        if (!task.has_value() && stealFn_)
        {
          task = stealFn_(id_);
        }

        if (!task.has_value())
        {
          idle_wait();
          continue;
        }

        execute_task(*task);
      }
    }

    /**
     * @brief Execute one task using a fresh scheduling budget.
     *
     * If the task yields, it is rescheduled locally while the worker is still
     * running. Once shutdown begins, yielded tasks are not resubmitted.
     *
     * @param task Task to execute.
     */
    void execute_task(Task &task)
    {
      if (!running())
      {
        return;
      }

      if (!task.valid() || task.done())
      {
        return;
      }

      Budget budget(budgetConfig_);
      budget.reset();

      while (running() && !budget.should_yield())
      {
        const TaskResult result = task.run();
        budget.consume();

        if (result == TaskResult::complete)
        {
          return;
        }

        if (result == TaskResult::failed)
        {
          return;
        }

        if (result == TaskResult::yield)
        {
          task.state = TaskState::yielded;

          if (!running())
          {
            return;
          }

          const bool resubmitted = submit(std::move(task));
          if (!resubmitted)
          {
            return;
          }

          return;
        }
      }

      if (!running())
      {
        return;
      }

      if (!task.done())
      {
        task.state = TaskState::yielded;

        const bool resubmitted = submit(std::move(task));
        if (!resubmitted)
        {
          return;
        }
      }
    }

    /**
     * @brief Sleep briefly when the worker has no work to do.
     *
     * V1 uses a tiny sleep to avoid hot spinning.
     */
    void idle_wait() const
    {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

  private:
    /** @brief Worker identifier. */
    std::uint32_t id_;

    /** @brief Local run queue owned by this worker. */
    RunQueue queue_;

    /** @brief Running state of the worker loop. */
    std::atomic<bool> running_;

    /** @brief Budget configuration used for task execution slices. */
    BudgetConfig budgetConfig_;

    /** @brief External callback used to steal work when idle. */
    StealFn stealFn_;

    /** @brief Underlying OS thread for this worker. */
    std::thread thread_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_WORKER_HPP
