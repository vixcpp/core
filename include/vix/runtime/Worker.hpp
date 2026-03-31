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
#include <vector>

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
   * - pops a local task
   * - executes it
   * - reschedules yielded tasks
   * - or tries to steal work if idle
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
          thread_(),
          executedTasks_(0),
          yieldedTasks_(0),
          failedTasks_(0),
          stolenTasks_(0),
          idleCycles_(0)
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
     * @brief Enqueue multiple tasks into the local run queue.
     *
     * @param tasks Tasks to enqueue.
     * @return Number of tasks accepted.
     */
    [[nodiscard]] std::size_t submit_batch(std::vector<Task> tasks)
    {
      if (!running())
      {
        return 0;
      }

      return queue_.push_batch(std::move(tasks));
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

    /**
     * @brief Return the total number of executed tasks.
     *
     * @return Number of tasks that entered execution.
     */
    [[nodiscard]] std::uint64_t executed_tasks() const noexcept
    {
      return executedTasks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Return the total number of yielded tasks.
     *
     * @return Number of yielded tasks rescheduled by this worker.
     */
    [[nodiscard]] std::uint64_t yielded_tasks() const noexcept
    {
      return yieldedTasks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Return the total number of failed tasks.
     *
     * @return Number of failed task executions observed by this worker.
     */
    [[nodiscard]] std::uint64_t failed_tasks() const noexcept
    {
      return failedTasks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Return the total number of stolen tasks executed here.
     *
     * @return Number of tasks obtained through stealing.
     */
    [[nodiscard]] std::uint64_t stolen_tasks() const noexcept
    {
      return stolenTasks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Return the total number of idle cycles.
     *
     * @return Number of times the worker observed no work.
     */
    [[nodiscard]] std::uint64_t idle_cycles() const noexcept
    {
      return idleCycles_.load(std::memory_order_relaxed);
    }

  private:
    /**
     * @brief Main worker loop.
     *
     * Strategy:
     * - first drain a small local batch to reduce lock traffic
     * - if empty, try one direct local pop
     * - then try stealing
     * - if still idle, use progressive backoff
     */
    void run_loop()
    {
      std::uint32_t idleStreak = 0;
      std::vector<Task> localBatch;
      localBatch.reserve(kLocalBatchSize);

      while (running())
      {
        if (localBatch.empty())
        {
          localBatch = queue_.try_pop_batch(kLocalBatchSize);
        }

        if (!localBatch.empty())
        {
          idleStreak = 0;

          Task task = std::move(localBatch.back());
          localBatch.pop_back();
          execute_task(task, false);
          continue;
        }

        if (auto task = queue_.try_pop(); task.has_value())
        {
          idleStreak = 0;
          execute_task(*task, false);
          continue;
        }

        if (stealFn_)
        {
          if (auto task = stealFn_(id_); task.has_value())
          {
            idleStreak = 0;
            execute_task(*task, true);
            continue;
          }
        }

        ++idleStreak;
        idleCycles_.fetch_add(1, std::memory_order_relaxed);
        idle_wait(idleStreak);
      }
    }

    /**
     * @brief Execute one task using a fresh scheduling budget.
     *
     * If the task yields, it is rescheduled locally while the worker is still
     * running. Once shutdown begins, yielded tasks are not resubmitted.
     *
     * @param task Task to execute.
     * @param stolen Whether the task came from stealing.
     */
    void execute_task(Task &task, bool stolen)
    {
      if (!running())
      {
        return;
      }

      if (!task.schedulable())
      {
        return;
      }

      if (stolen)
      {
        stolenTasks_.fetch_add(1, std::memory_order_relaxed);
      }

      Budget budget(budgetConfig_);
      executedTasks_.fetch_add(1, std::memory_order_relaxed);

      while (running() && budget.available())
      {
        const TaskResult result = task.run();
        budget.consume();

        if (result == TaskResult::complete)
        {
          return;
        }

        if (result == TaskResult::failed)
        {
          failedTasks_.fetch_add(1, std::memory_order_relaxed);
          return;
        }

        if (result == TaskResult::yield)
        {
          yieldedTasks_.fetch_add(1, std::memory_order_relaxed);

          if (!running())
          {
            return;
          }

          task.mark_ready();
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
        yieldedTasks_.fetch_add(1, std::memory_order_relaxed);
        task.mark_ready();

        const bool resubmitted = submit(std::move(task));
        if (!resubmitted)
        {
          return;
        }
      }
    }

    /**
     * @brief Wait briefly when the worker has no work to do.
     *
     * This uses a progressive strategy:
     * - first a few cheap yields
     * - then a short pause
     * - then a very short sleep
     *
     * This keeps the worker responsive for bursty HTTP workloads while still
     * avoiding permanent hot spinning.
     *
     * @param idleStreak Number of consecutive idle observations.
     */
    void idle_wait(std::uint32_t idleStreak) const
    {
      if (idleStreak <= 32u)
      {
        std::this_thread::yield();
        return;
      }

      if (idleStreak <= 128u)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(2));
        return;
      }

      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

  private:
    static constexpr std::size_t kLocalBatchSize = 8u;

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

    /** @brief Number of tasks executed by this worker. */
    std::atomic<std::uint64_t> executedTasks_;

    /** @brief Number of yielded tasks observed by this worker. */
    std::atomic<std::uint64_t> yieldedTasks_;

    /** @brief Number of failed tasks observed by this worker. */
    std::atomic<std::uint64_t> failedTasks_;

    /** @brief Number of stolen tasks executed by this worker. */
    std::atomic<std::uint64_t> stolenTasks_;

    /** @brief Number of idle cycles observed by this worker. */
    std::atomic<std::uint64_t> idleCycles_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_WORKER_HPP
