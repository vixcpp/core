/**
 *
 * @file Task.hpp
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
#ifndef VIX_RUNTIME_TASK_HPP
#define VIX_RUNTIME_TASK_HPP

#include <cstdint>
#include <exception>
#include <functional>
#include <utility>

namespace vix::runtime
{

  /**
   * @brief Strong identifier type for runtime tasks.
   */
  using TaskId = std::uint64_t;

  /**
   * @brief Result returned by a task execution step.
   *
   * A task may either:
   * - complete its work
   * - yield and request rescheduling
   * - or fail
   */
  enum class TaskResult : std::uint8_t
  {
    complete = 0,
    yield = 1,
    failed = 2
  };

  /**
   * @brief Lifecycle state of a runtime task.
   */
  enum class TaskState : std::uint8_t
  {
    ready = 0,
    running = 1,
    yielded = 2,
    completed = 3,
    failed = 4,
    cancelled = 5
  };

  /**
   * @brief Callable type executed by the runtime scheduler.
   *
   * The callable should do a small unit of work and return:
   * - TaskResult::complete when finished
   * - TaskResult::yield when it should be scheduled again
   * - TaskResult::failed when it failed explicitly
   */
  using TaskFn = std::function<TaskResult()>;

  /**
   * @brief Lightweight schedulable runtime task.
   *
   * This is the base unit executed by the Vix runtime scheduler.
   * It is intentionally minimal for V1:
   * - an id
   * - a callable
   * - a state
   * - an optional affinity hint
   * - an optional last error
   */
  struct Task
  {
    /** @brief Unique task identifier. */
    TaskId id;

    /** @brief Task callable executed by a worker. */
    TaskFn fn;

    /** @brief Current lifecycle state. */
    TaskState state;

    /**
     * @brief Optional worker affinity hint.
     *
     * A value of 0 means "no specific preference" for V1.
     */
    std::uint32_t affinity;

    /** @brief Last captured exception, if any. */
    std::exception_ptr error;

    /**
     * @brief Construct an empty invalid task.
     */
    Task() noexcept
        : id(0),
          fn(),
          state(TaskState::ready),
          affinity(0),
          error(nullptr)
    {
    }

    /**
     * @brief Construct a task from an id and callable.
     *
     * @param taskId Unique task identifier.
     * @param taskFn Task callable.
     * @param workerAffinity Optional worker affinity hint.
     */
    Task(TaskId taskId, TaskFn taskFn, std::uint32_t workerAffinity = 0) noexcept
        : id(taskId),
          fn(std::move(taskFn)),
          state(TaskState::ready),
          affinity(workerAffinity),
          error(nullptr)
    {
    }

    /**
     * @brief Check whether the task contains executable work.
     *
     * @return true if the callable is valid, false otherwise.
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return static_cast<bool>(fn);
    }

    /**
     * @brief Check whether the task is ready to be scheduled.
     *
     * @return true if the task may be executed by the runtime.
     */
    [[nodiscard]] bool schedulable() const noexcept
    {
      return valid() &&
             state != TaskState::running &&
             state != TaskState::completed &&
             state != TaskState::failed &&
             state != TaskState::cancelled;
    }

    /**
     * @brief Check whether the task reached a terminal state.
     *
     * @return true if the task is completed, failed, or cancelled.
     */
    [[nodiscard]] bool done() const noexcept
    {
      return state == TaskState::completed ||
             state == TaskState::failed ||
             state == TaskState::cancelled;
    }

    /**
     * @brief Check whether the task is currently running.
     *
     * @return true if the task is in the running state.
     */
    [[nodiscard]] bool running() const noexcept
    {
      return state == TaskState::running;
    }

    /**
     * @brief Check whether the task yielded and expects rescheduling.
     *
     * @return true if the task is in the yielded state.
     */
    [[nodiscard]] bool yielded() const noexcept
    {
      return state == TaskState::yielded;
    }

    /**
     * @brief Mark the task as ready for scheduling.
     *
     * This is useful when the scheduler or worker wants to normalize the state
     * before placing the task back into a queue.
     */
    void mark_ready() noexcept
    {
      if (!done())
      {
        state = TaskState::ready;
      }
    }

    /**
     * @brief Cancel the task before execution.
     *
     * Cancellation only changes the state if the task is not already terminal.
     */
    void cancel() noexcept
    {
      if (!done())
      {
        state = TaskState::cancelled;
      }
    }

    /**
     * @brief Clear the last captured error.
     */
    void clear_error() noexcept
    {
      error = nullptr;
    }

    /**
     * @brief Execute one scheduling step of the task.
     *
     * The callable is expected to perform a small slice of work and return
     * whether it is finished or should be rescheduled.
     *
     * If the callable throws, the task is marked as failed and the exception
     * is captured in @ref error.
     *
     * Rules:
     * - an invalid task fails immediately
     * - a cancelled task is treated as failed to execute
     * - a terminal task is not re-executed
     *
     * @return TaskResult Result of the execution step.
     */
    [[nodiscard]] TaskResult run() noexcept
    {
      if (!valid())
      {
        state = TaskState::failed;
        return TaskResult::failed;
      }

      if (state == TaskState::cancelled)
      {
        return TaskResult::failed;
      }

      if (state == TaskState::completed || state == TaskState::failed)
      {
        return TaskResult::failed;
      }

      clear_error();
      state = TaskState::running;

      try
      {
        const TaskResult result = fn();

        switch (result)
        {
        case TaskResult::complete:
          state = TaskState::completed;
          return TaskResult::complete;

        case TaskResult::yield:
          state = TaskState::yielded;
          return TaskResult::yield;

        case TaskResult::failed:
        default:
          state = TaskState::failed;
          return TaskResult::failed;
        }
      }
      catch (...)
      {
        error = std::current_exception();
        state = TaskState::failed;
        return TaskResult::failed;
      }
    }
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_TASK_HPP
