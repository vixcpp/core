/**
 *
 * @file RunQueue.hpp
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
#ifndef VIX_RUNTIME_RUN_QUEUE_HPP
#define VIX_RUNTIME_RUN_QUEUE_HPP

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <vix/runtime/Task.hpp>

namespace vix::runtime
{

  /**
   * @brief Local run queue used by a runtime worker.
   *
   * Design:
   * - local worker pushes and pops from the front
   * - stealing happens from the back
   *
   * This keeps the queue simple and predictable while preserving decent locality
   * for the owning worker.
   */
  class RunQueue
  {
  public:
    /**
     * @brief Construct an empty run queue.
     */
    RunQueue() = default;

    RunQueue(const RunQueue &) = delete;
    RunQueue &operator=(const RunQueue &) = delete;

    RunQueue(RunQueue &&) = delete;
    RunQueue &operator=(RunQueue &&) = delete;

    /**
     * @brief Push one task into the local queue.
     *
     * Local tasks are inserted at the front so the owning worker can continue
     * processing recent work with low overhead.
     *
     * The queue only accepts schedulable tasks.
     * The state is normalized to @ref TaskState::ready before insertion.
     *
     * @param task Task to enqueue.
     * @return true if the task was enqueued, false otherwise.
     */
    [[nodiscard]] bool push(Task task)
    {
      if (!task.schedulable())
      {
        return false;
      }

      task.mark_ready();

      std::lock_guard<std::mutex> lock(mutex_);
      queue_.emplace_front(std::move(task));
      return true;
    }

    /**
     * @brief Push many tasks into the local queue with a single lock.
     *
     * This is useful when a worker or scheduler needs to enqueue several tasks
     * at once and wants to reduce mutex traffic.
     *
     * Tasks are inserted in iteration order at the front, which preserves the
     * "most recently pushed is processed first" local-worker behavior.
     *
     * @param tasks Batch of tasks to enqueue.
     * @return Number of tasks successfully enqueued.
     */
    [[nodiscard]] std::size_t push_batch(std::vector<Task> tasks)
    {
      std::size_t accepted = 0;

      std::lock_guard<std::mutex> lock(mutex_);
      for (auto &task : tasks)
      {
        if (!task.schedulable())
        {
          continue;
        }

        task.mark_ready();
        queue_.emplace_front(std::move(task));
        ++accepted;
      }

      return accepted;
    }

    /**
     * @brief Pop one task for the owning worker.
     *
     * The owning worker pops from the front of the deque.
     *
     * @return The next task if available, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<Task> try_pop()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return pop_front_locked();
    }

    /**
     * @brief Steal one task from the queue.
     *
     * A foreign worker steals from the back to reduce contention with the local
     * worker that pops from the front.
     *
     * @return A stolen task if available, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<Task> try_steal()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return pop_back_locked();
    }

    /**
     * @brief Pop up to @p max_count tasks from the front.
     *
     * This is useful when a worker wants to refill a local scratch buffer and
     * avoid repeated lock/unlock cycles.
     *
     * @param max_count Maximum number of tasks to pop.
     * @return Vector containing up to max_count tasks.
     */
    [[nodiscard]] std::vector<Task> try_pop_batch(std::size_t max_count)
    {
      std::vector<Task> out;
      if (max_count == 0)
      {
        return out;
      }

      std::lock_guard<std::mutex> lock(mutex_);
      const std::size_t count = (max_count < queue_.size()) ? max_count : queue_.size();
      out.reserve(count);

      for (std::size_t i = 0; i < count; ++i)
      {
        Task task = std::move(queue_.front());
        queue_.pop_front();
        out.emplace_back(std::move(task));
      }

      return out;
    }

    /**
     * @brief Return the next local task without removing it.
     *
     * This inspects the front of the queue.
     *
     * @return The next local task if available, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<Task> front() const
    {
      std::lock_guard<std::mutex> lock(mutex_);

      if (queue_.empty())
      {
        return std::nullopt;
      }

      return queue_.front();
    }

    /**
     * @brief Return the next stealable task without removing it.
     *
     * This inspects the back of the queue.
     *
     * @return The next stealable task if available, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<Task> back() const
    {
      std::lock_guard<std::mutex> lock(mutex_);

      if (queue_.empty())
      {
        return std::nullopt;
      }

      return queue_.back();
    }

    /**
     * @brief Check whether the queue is empty.
     *
     * @return true if the queue contains no task.
     */
    [[nodiscard]] bool empty() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.empty();
    }

    /**
     * @brief Return the current number of tasks in the queue.
     *
     * @return Number of enqueued tasks.
     */
    [[nodiscard]] std::size_t size() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.size();
    }

    /**
     * @brief Remove all tasks from the queue.
     *
     * @return Number of tasks removed.
     */
    [[nodiscard]] std::size_t clear()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const std::size_t count = queue_.size();
      queue_.clear();
      return count;
    }

    /**
     * @brief Swap this queue with another one.
     *
     * Useful for future optimizations where a worker may want to quickly exchange
     * queue contents with a temporary local buffer.
     *
     * @param other Queue to swap with.
     */
    void swap(RunQueue &other) noexcept
    {
      if (this == &other)
      {
        return;
      }

      std::scoped_lock lock(mutex_, other.mutex_);
      queue_.swap(other.queue_);
    }

  private:
    /**
     * @brief Pop one task from the front.
     *
     * Caller must already hold the mutex.
     *
     * @return The next task if available.
     */
    [[nodiscard]] std::optional<Task> pop_front_locked()
    {
      if (queue_.empty())
      {
        return std::nullopt;
      }

      Task task = std::move(queue_.front());
      queue_.pop_front();
      return task;
    }

    /**
     * @brief Pop one task from the back.
     *
     * Caller must already hold the mutex.
     *
     * @return The next stolen task if available.
     */
    [[nodiscard]] std::optional<Task> pop_back_locked()
    {
      if (queue_.empty())
      {
        return std::nullopt;
      }

      Task task = std::move(queue_.back());
      queue_.pop_back();
      return task;
    }

  private:
    /** @brief Protected task storage. */
    std::deque<Task> queue_;

    /** @brief Mutex protecting the queue. */
    mutable std::mutex mutex_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_RUN_QUEUE_HPP
