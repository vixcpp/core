/**
 *
 * @file Scheduler.hpp
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
#ifndef VIX_RUNTIME_SCHEDULER_HPP
#define VIX_RUNTIME_SCHEDULER_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Task.hpp>
#include <vix/runtime/Worker.hpp>

namespace vix::runtime
{

  /**
   * @brief Lightweight user-space task scheduler.
   *
   * The scheduler owns a set of workers and is responsible for:
   * - starting and stopping them,
   * - dispatching submitted tasks,
   * - providing work stealing across workers.
   */
  class Scheduler
  {
  public:
    /**
     * @brief Construct a scheduler with a fixed number of workers.
     *
     * @param workerCount Number of worker threads to create.
     * @param budgetConfig Per-worker task budget configuration.
     */
    explicit Scheduler(std::uint32_t workerCount,
                       const BudgetConfig &budgetConfig = BudgetConfig{})
        : workers_(),
          nextWorker_(0),
          running_(false)
    {
      if (workerCount == 0)
      {
        workerCount = 1;
      }

      workers_.reserve(static_cast<std::size_t>(workerCount));

      for (std::uint32_t i = 0; i < workerCount; ++i)
      {
        workers_.push_back(std::make_unique<Worker>(i, budgetConfig));
      }

      bind_steal_callbacks();
    }

    Scheduler(const Scheduler &) = delete;
    Scheduler &operator=(const Scheduler &) = delete;

    Scheduler(Scheduler &&) = delete;
    Scheduler &operator=(Scheduler &&) = delete;

    /**
     * @brief Stop all workers on destruction.
     */
    ~Scheduler()
    {
      stop();
    }

    /**
     * @brief Start all workers.
     *
     * Has no effect if already started.
     */
    void start()
    {
      bool expected = false;
      if (!running_.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
      {
        return;
      }

      for (auto &worker : workers_)
      {
        worker->start();
      }
    }

    /**
     * @brief Stop all workers and join their threads.
     *
     * Has no effect if already stopped.
     */
    void stop()
    {
      bool expected = true;
      if (!running_.compare_exchange_strong(expected, false,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
      {
        return;
      }

      for (auto &worker : workers_)
      {
        worker->stop();
      }

      for (auto &worker : workers_)
      {
        worker->join();
      }
    }

    /**
     * @brief Return whether the scheduler is running.
     *
     * @return true if all workers are intended to be active.
     */
    [[nodiscard]] bool running() const noexcept
    {
      return running_.load(std::memory_order_acquire);
    }

    /**
     * @brief Return the number of workers owned by the scheduler.
     *
     * @return Worker count.
     */
    [[nodiscard]] std::uint32_t worker_count() const noexcept
    {
      return static_cast<std::uint32_t>(workers_.size());
    }

    /**
     * @brief Submit a task to the scheduler.
     *
     * Dispatch policy:
     * - if affinity is valid, use it,
     * - otherwise use round-robin.
     *
     * @param task Task to schedule.
     * @return true if submitted successfully, false otherwise.
     */
    [[nodiscard]] bool submit(Task task)
    {
      if (!task.valid() || workers_.empty())
      {
        return false;
      }

      Worker *worker = select_worker(task);
      if (worker == nullptr)
      {
        return false;
      }

      if (task.state != TaskState::cancelled &&
          task.state != TaskState::completed &&
          task.state != TaskState::failed)
      {
        task.state = TaskState::ready;
      }

      return worker->submit(std::move(task));
    }

    /**
     * @brief Try to steal work for one worker from the others.
     *
     * The requesting worker never steals from itself.
     *
     * @param thiefId Worker id requesting work.
     * @return One stolen task if available.
     */
    [[nodiscard]] std::optional<Task> try_steal(std::uint32_t thiefId)
    {
      const std::size_t count = workers_.size();
      if (count <= 1)
      {
        return std::nullopt;
      }

      const std::size_t start =
          static_cast<std::size_t>(thiefId + 1u) % count;

      for (std::size_t offset = 0; offset < count - 1; ++offset)
      {
        const std::size_t victimIndex = (start + offset) % count;
        const std::uint32_t victimId = static_cast<std::uint32_t>(victimIndex);

        if (victimId == thiefId)
        {
          continue;
        }

        if (auto task = workers_[victimIndex]->try_steal(); task.has_value())
        {
          return task;
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Return the total number of queued tasks across all workers.
     *
     * @return Total queued task count.
     */
    [[nodiscard]] std::size_t size() const
    {
      std::size_t total = 0;

      for (const auto &worker : workers_)
      {
        total += worker->size();
      }

      return total;
    }

    /**
     * @brief Return whether all worker queues are empty.
     *
     * @return true if there is no queued task.
     */
    [[nodiscard]] bool empty() const
    {
      for (const auto &worker : workers_)
      {
        if (!worker->empty())
        {
          return false;
        }
      }

      return true;
    }

  private:
    /**
     * @brief Bind each worker steal callback to this scheduler instance.
     */
    void bind_steal_callbacks()
    {
      for (auto &worker : workers_)
      {
        worker->set_steal_callback(
            [this](std::uint32_t workerId) -> std::optional<Task>
            {
              return this->try_steal(workerId);
            });
      }
    }

    /**
     * @brief Select the destination worker for a task.
     *
     * @param task Task to place.
     * @return Pointer to the selected worker, or nullptr on failure.
     */
    [[nodiscard]] Worker *select_worker(const Task &task)
    {
      if (workers_.empty())
      {
        return nullptr;
      }

      if (task.affinity > 0)
      {
        const std::size_t index =
            static_cast<std::size_t>(task.affinity) % workers_.size();
        return workers_[index].get();
      }

      const std::size_t index =
          static_cast<std::size_t>(
              nextWorker_.fetch_add(1, std::memory_order_relaxed)) %
          workers_.size();

      return workers_[index].get();
    }

  private:
    /** @brief Owned runtime workers. */
    std::vector<std::unique_ptr<Worker>> workers_;

    /** @brief Round-robin dispatch cursor. */
    std::atomic<std::uint32_t> nextWorker_;

    /** @brief Global scheduler running flag. */
    std::atomic<bool> running_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_SCHEDULER_HPP
