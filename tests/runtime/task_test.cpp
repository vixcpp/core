/**
 *
 * @file task_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <cassert>
#include <cstdint>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <vix/runtime/Task.hpp>

namespace
{
  using Task = vix::runtime::Task;
  using TaskFn = vix::runtime::TaskFn;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;

  static void test_task_result_numeric_values_are_stable()
  {
    static_assert(static_cast<std::uint8_t>(TaskResult::complete) == 0u);
    static_assert(static_cast<std::uint8_t>(TaskResult::yield) == 1u);
    static_assert(static_cast<std::uint8_t>(TaskResult::failed) == 2u);
  }

  static void test_task_state_numeric_values_are_stable()
  {
    static_assert(static_cast<std::uint8_t>(TaskState::ready) == 0u);
    static_assert(static_cast<std::uint8_t>(TaskState::running) == 1u);
    static_assert(static_cast<std::uint8_t>(TaskState::yielded) == 2u);
    static_assert(static_cast<std::uint8_t>(TaskState::completed) == 3u);
    static_assert(static_cast<std::uint8_t>(TaskState::failed) == 4u);
    static_assert(static_cast<std::uint8_t>(TaskState::cancelled) == 5u);
  }

  static void test_default_task_is_invalid_but_ready()
  {
    Task task;

    assert(task.id == 0u);
    assert(!task.fn);
    assert(task.state == TaskState::ready);
    assert(task.affinity == 0u);
    assert(task.error == nullptr);

    assert(!task.valid());
    assert(!task.schedulable());
    assert(!task.done());
    assert(!task.running());
    assert(!task.yielded());
  }

  static void test_construct_valid_task()
  {
    int calls = 0;

    Task task{
        42u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        }};

    assert(task.id == 42u);
    assert(task.fn);
    assert(task.state == TaskState::ready);
    assert(task.affinity == 0u);
    assert(task.error == nullptr);

    assert(task.valid());
    assert(task.schedulable());
    assert(!task.done());
    assert(!task.running());
    assert(!task.yielded());
    assert(calls == 0);
  }

  static void test_construct_task_with_affinity()
  {
    Task task{
        7u,
        []()
        {
          return TaskResult::complete;
        },
        3u};

    assert(task.id == 7u);
    assert(task.affinity == 3u);
    assert(task.valid());
    assert(task.schedulable());
    assert(task.state == TaskState::ready);
  }

  static void test_valid_returns_false_for_empty_callable()
  {
    Task task{
        1u,
        TaskFn{}};

    assert(task.id == 1u);
    assert(!task.valid());
    assert(!task.schedulable());
    assert(!task.done());
  }

  static void test_schedulable_for_ready_task()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::ready;

    assert(task.valid());
    assert(task.schedulable());
    assert(!task.done());
  }

  static void test_schedulable_for_yielded_task()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::yielded;

    assert(task.valid());
    assert(task.schedulable());
    assert(!task.done());
    assert(task.yielded());
  }

  static void test_schedulable_rejects_running_task()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::running;

    assert(task.valid());
    assert(!task.schedulable());
    assert(!task.done());
    assert(task.running());
  }

  static void test_schedulable_rejects_completed_task()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::completed;

    assert(task.valid());
    assert(!task.schedulable());
    assert(task.done());
  }

  static void test_schedulable_rejects_failed_task()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::failed;

    assert(task.valid());
    assert(!task.schedulable());
    assert(task.done());
  }

  static void test_schedulable_rejects_cancelled_task()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::cancelled;

    assert(task.valid());
    assert(!task.schedulable());
    assert(task.done());
  }

  static void test_done_detects_terminal_states()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::ready;
    assert(!task.done());

    task.state = TaskState::running;
    assert(!task.done());

    task.state = TaskState::yielded;
    assert(!task.done());

    task.state = TaskState::completed;
    assert(task.done());

    task.state = TaskState::failed;
    assert(task.done());

    task.state = TaskState::cancelled;
    assert(task.done());
  }

  static void test_running_detects_running_state_only()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::ready;
    assert(!task.running());

    task.state = TaskState::running;
    assert(task.running());

    task.state = TaskState::yielded;
    assert(!task.running());

    task.state = TaskState::completed;
    assert(!task.running());

    task.state = TaskState::failed;
    assert(!task.running());

    task.state = TaskState::cancelled;
    assert(!task.running());
  }

  static void test_yielded_detects_yielded_state_only()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::ready;
    assert(!task.yielded());

    task.state = TaskState::running;
    assert(!task.yielded());

    task.state = TaskState::yielded;
    assert(task.yielded());

    task.state = TaskState::completed;
    assert(!task.yielded());

    task.state = TaskState::failed;
    assert(!task.yielded());

    task.state = TaskState::cancelled;
    assert(!task.yielded());
  }

  static void test_mark_ready_changes_non_terminal_states_to_ready()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::running;
    task.mark_ready();
    assert(task.state == TaskState::ready);

    task.state = TaskState::yielded;
    task.mark_ready();
    assert(task.state == TaskState::ready);

    task.state = TaskState::ready;
    task.mark_ready();
    assert(task.state == TaskState::ready);
  }

  static void test_mark_ready_does_not_change_terminal_states()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::completed;
    task.mark_ready();
    assert(task.state == TaskState::completed);

    task.state = TaskState::failed;
    task.mark_ready();
    assert(task.state == TaskState::failed);

    task.state = TaskState::cancelled;
    task.mark_ready();
    assert(task.state == TaskState::cancelled);
  }

  static void test_cancel_changes_non_terminal_states_to_cancelled()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::ready;
    task.cancel();
    assert(task.state == TaskState::cancelled);
    assert(task.done());

    task.state = TaskState::running;
    task.cancel();
    assert(task.state == TaskState::cancelled);
    assert(task.done());

    task.state = TaskState::yielded;
    task.cancel();
    assert(task.state == TaskState::cancelled);
    assert(task.done());
  }

  static void test_cancel_does_not_change_terminal_states()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    task.state = TaskState::completed;
    task.cancel();
    assert(task.state == TaskState::completed);

    task.state = TaskState::failed;
    task.cancel();
    assert(task.state == TaskState::failed);

    task.state = TaskState::cancelled;
    task.cancel();
    assert(task.state == TaskState::cancelled);
  }

  static void test_clear_error_resets_exception_pointer()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    try
    {
      throw std::runtime_error("stored error");
    }
    catch (...)
    {
      task.error = std::current_exception();
    }

    assert(task.error != nullptr);

    task.clear_error();

    assert(task.error == nullptr);
  }

  static void test_run_invalid_task_fails()
  {
    Task task;

    const TaskResult result = task.run();

    assert(result == TaskResult::failed);
    assert(task.state == TaskState::failed);
    assert(task.done());
    assert(task.error == nullptr);
  }

  static void test_run_complete_task()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        }};

    const TaskResult result = task.run();

    assert(result == TaskResult::complete);
    assert(calls == 1);
    assert(task.state == TaskState::completed);
    assert(task.done());
    assert(!task.schedulable());
    assert(!task.running());
    assert(!task.yielded());
    assert(task.error == nullptr);
  }

  static void test_run_yielding_task()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::yield;
        }};

    const TaskResult result = task.run();

    assert(result == TaskResult::yield);
    assert(calls == 1);
    assert(task.state == TaskState::yielded);
    assert(!task.done());
    assert(task.schedulable());
    assert(!task.running());
    assert(task.yielded());
    assert(task.error == nullptr);
  }

  static void test_run_failed_task_result()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::failed;
        }};

    const TaskResult result = task.run();

    assert(result == TaskResult::failed);
    assert(calls == 1);
    assert(task.state == TaskState::failed);
    assert(task.done());
    assert(!task.schedulable());
    assert(task.error == nullptr);
  }

  static void test_run_callable_exception_marks_failed_and_stores_error()
  {
    Task task{
        1u,
        []() -> TaskResult
        {
          throw std::runtime_error("boom");
        }};

    const TaskResult result = task.run();

    assert(result == TaskResult::failed);
    assert(task.state == TaskState::failed);
    assert(task.done());
    assert(!task.schedulable());
    assert(task.error != nullptr);

    bool caught = false;

    try
    {
      std::rethrow_exception(task.error);
    }
    catch (const std::runtime_error &e)
    {
      caught = true;
      assert(std::string(e.what()) == "boom");
    }

    assert(caught);
  }

  static void test_run_clears_previous_error_before_success()
  {
    Task task{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    try
    {
      throw std::runtime_error("old error");
    }
    catch (...)
    {
      task.error = std::current_exception();
    }

    assert(task.error != nullptr);

    const TaskResult result = task.run();

    assert(result == TaskResult::complete);
    assert(task.state == TaskState::completed);
    assert(task.error == nullptr);
  }

  static void test_run_cancelled_task_does_not_execute_callable()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        }};

    task.cancel();

    const TaskResult result = task.run();

    assert(result == TaskResult::failed);
    assert(calls == 0);
    assert(task.state == TaskState::cancelled);
    assert(task.done());
  }

  static void test_run_completed_task_does_not_execute_again()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        }};

    assert(task.run() == TaskResult::complete);
    assert(calls == 1);
    assert(task.state == TaskState::completed);

    const TaskResult second = task.run();

    assert(second == TaskResult::failed);
    assert(calls == 1);
    assert(task.state == TaskState::completed);
  }

  static void test_run_failed_task_does_not_execute_again()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::failed;
        }};

    assert(task.run() == TaskResult::failed);
    assert(calls == 1);
    assert(task.state == TaskState::failed);

    const TaskResult second = task.run();

    assert(second == TaskResult::failed);
    assert(calls == 1);
    assert(task.state == TaskState::failed);
  }

  static void test_yielded_task_can_run_again()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;

          if (calls == 1)
          {
            return TaskResult::yield;
          }

          return TaskResult::complete;
        }};

    assert(task.run() == TaskResult::yield);
    assert(calls == 1);
    assert(task.state == TaskState::yielded);
    assert(task.schedulable());

    assert(task.run() == TaskResult::complete);
    assert(calls == 2);
    assert(task.state == TaskState::completed);
    assert(task.done());
  }

  static void test_mark_ready_after_yield_then_run_complete()
  {
    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;

          if (calls == 1)
          {
            return TaskResult::yield;
          }

          return TaskResult::complete;
        }};

    assert(task.run() == TaskResult::yield);
    assert(task.state == TaskState::yielded);

    task.mark_ready();

    assert(task.state == TaskState::ready);
    assert(task.schedulable());

    assert(task.run() == TaskResult::complete);
    assert(calls == 2);
    assert(task.state == TaskState::completed);
  }

  static void test_unknown_task_result_defaults_to_failed()
  {
    Task task{
        1u,
        []()
        {
          return static_cast<TaskResult>(255u);
        }};

    const TaskResult result = task.run();

    assert(result == TaskResult::failed);
    assert(task.state == TaskState::failed);
    assert(task.done());
  }

  static void test_task_can_be_copied()
  {
    int calls = 0;

    Task source{
        10u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        },
        2u};

    Task copy = source;

    assert(copy.id == 10u);
    assert(copy.affinity == 2u);
    assert(copy.state == TaskState::ready);
    assert(copy.valid());
    assert(copy.schedulable());

    assert(source.run() == TaskResult::complete);
    assert(copy.run() == TaskResult::complete);

    assert(calls == 2);
  }

  static void test_task_copy_assignment()
  {
    int calls = 0;

    Task source{
        11u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        },
        4u};

    Task target;

    target = source;

    assert(target.id == 11u);
    assert(target.affinity == 4u);
    assert(target.state == TaskState::ready);
    assert(target.valid());

    assert(target.run() == TaskResult::complete);
    assert(calls == 1);
  }

  static void test_task_can_be_moved()
  {
    int calls = 0;

    Task source{
        12u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        },
        5u};

    Task moved = std::move(source);

    assert(moved.id == 12u);
    assert(moved.affinity == 5u);
    assert(moved.state == TaskState::ready);
    assert(moved.valid());

    assert(moved.run() == TaskResult::complete);
    assert(calls == 1);
  }

  static void test_task_move_assignment()
  {
    int calls = 0;

    Task source{
        13u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        },
        6u};

    Task target;

    target = std::move(source);

    assert(target.id == 13u);
    assert(target.affinity == 6u);
    assert(target.state == TaskState::ready);
    assert(target.valid());

    assert(target.run() == TaskResult::complete);
    assert(calls == 1);
  }

  static void test_task_preserves_exception_pointer_on_copy()
  {
    Task source{
        1u,
        []()
        {
          return TaskResult::complete;
        }};

    try
    {
      throw std::runtime_error("copy error");
    }
    catch (...)
    {
      source.error = std::current_exception();
    }

    Task copy = source;

    assert(source.error != nullptr);
    assert(copy.error != nullptr);

    bool caught = false;

    try
    {
      std::rethrow_exception(copy.error);
    }
    catch (const std::runtime_error &e)
    {
      caught = true;
      assert(std::string(e.what()) == "copy error");
    }

    assert(caught);
  }

  static void test_large_task_id_is_supported()
  {
    constexpr TaskId large_id = static_cast<TaskId>(0xffffffffffffffffull);

    Task task{
        large_id,
        []()
        {
          return TaskResult::complete;
        }};

    assert(task.id == large_id);
    assert(task.valid());
    assert(task.run() == TaskResult::complete);
  }

  static void test_task_type_traits()
  {
    static_assert(std::is_same_v<TaskId, std::uint64_t>);
    static_assert(std::is_same_v<TaskFn, std::function<TaskResult()>>);

    static_assert(std::is_default_constructible_v<Task>);
    static_assert(std::is_constructible_v<Task, TaskId, TaskFn>);
    static_assert(std::is_constructible_v<Task, TaskId, TaskFn, std::uint32_t>);

    static_assert(std::is_copy_constructible_v<Task>);
    static_assert(std::is_copy_assignable_v<Task>);
    static_assert(std::is_move_constructible_v<Task>);
    static_assert(std::is_move_assignable_v<Task>);
    static_assert(std::is_destructible_v<Task>);
  }

} // namespace

int main()
{
  test_task_result_numeric_values_are_stable();
  test_task_state_numeric_values_are_stable();

  test_default_task_is_invalid_but_ready();
  test_construct_valid_task();
  test_construct_task_with_affinity();
  test_valid_returns_false_for_empty_callable();

  test_schedulable_for_ready_task();
  test_schedulable_for_yielded_task();
  test_schedulable_rejects_running_task();
  test_schedulable_rejects_completed_task();
  test_schedulable_rejects_failed_task();
  test_schedulable_rejects_cancelled_task();

  test_done_detects_terminal_states();
  test_running_detects_running_state_only();
  test_yielded_detects_yielded_state_only();

  test_mark_ready_changes_non_terminal_states_to_ready();
  test_mark_ready_does_not_change_terminal_states();

  test_cancel_changes_non_terminal_states_to_cancelled();
  test_cancel_does_not_change_terminal_states();

  test_clear_error_resets_exception_pointer();

  test_run_invalid_task_fails();
  test_run_complete_task();
  test_run_yielding_task();
  test_run_failed_task_result();
  test_run_callable_exception_marks_failed_and_stores_error();
  test_run_clears_previous_error_before_success();

  test_run_cancelled_task_does_not_execute_callable();
  test_run_completed_task_does_not_execute_again();
  test_run_failed_task_does_not_execute_again();

  test_yielded_task_can_run_again();
  test_mark_ready_after_yield_then_run_complete();

  test_unknown_task_result_defaults_to_failed();

  test_task_can_be_copied();
  test_task_copy_assignment();
  test_task_can_be_moved();
  test_task_move_assignment();
  test_task_preserves_exception_pointer_on_copy();

  test_large_task_id_is_supported();

  test_task_type_traits();

  return 0;
}
