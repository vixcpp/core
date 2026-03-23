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

#include <vix/runtime/Task.hpp>

namespace vix::runtime
{

  /**
   * @brief Local run queue used by a runtime worker.
   *
   * V1 uses a simple double-ended queue protected by a mutex:
   * - local worker pushes and pops from the front
   * - stealing happens from the back
   *
   * This keeps the design easy to reason about and sufficient for the first
   * runtime version.
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
     * @brief Push a task into the local queue.
     *
     * Local tasks are inserted at the front so the owning worker can continue
     * processing recent work with low overhead.
     *
     * @param task Task to enqueue.
     * @return true if the task was enqueued, false if invalid.
     */
    [[nodiscard]] bool push(Task task)
    {
      if (!task.valid())
      {
        return false;
      }

      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push_front(std::move(task));
      return true;
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

      if (queue_.empty())
      {
        return std::nullopt;
      }

      Task task = std::move(queue_.front());
      queue_.pop_front();
      return task;
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

      if (queue_.empty())
      {
        return std::nullopt;
      }

      Task task = std::move(queue_.back());
      queue_.pop_back();
      return task;
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
     */
    void clear()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.clear();
    }

  private:
    /** @brief Protected task storage. */
    std::deque<Task> queue_;

    /** @brief Mutex protecting the queue. */
    mutable std::mutex mutex_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_RUN_QUEUE_HPP
