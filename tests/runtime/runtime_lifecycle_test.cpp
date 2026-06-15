/**
 *
 * @file runtime_lifecycle_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/runtime/Task.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Runtime = vix::runtime::Runtime;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
  using Task = vix::runtime::Task;
  using TaskFn = vix::runtime::TaskFn;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;

  using namespace std::chrono_literals;

  template <class Predicate>
  static bool wait_until(
      Predicate predicate,
      std::chrono::milliseconds timeout = 1000ms)
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
      if (predicate())
      {
        return true;
      }

      std::this_thread::sleep_for(1ms);
    }

    return predicate();
  }

  static Task make_complete_task(
      TaskId id,
      std::atomic<int> &counter,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::complete;
        },
        affinity};
  }

  static Task make_failed_task(
      TaskId id,
      std::atomic<int> &counter,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::failed;
        },
        affinity};
  }

  static Task make_throwing_task(
      TaskId id,
      std::atomic<int> &counter,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&counter]() -> TaskResult
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          throw std::runtime_error("runtime task failed");
        },
        affinity};
  }

  static Task make_invalid_task(TaskId id = 0)
  {
    return Task{
        id,
        TaskFn{}};
  }

  static Runtime make_runtime(
      std::uint32_t workers = 1u,
      std::uint32_t quantum = 8u)
  {
    return Runtime{
        RuntimeConfig{
            workers,
            BudgetConfig{quantum}}};
  }

  static void stop_runtime(Runtime &runtime)
  {
    runtime.stop();

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_runtime_type_traits()
  {
    static_assert(std::is_constructible_v<Runtime>);
    static_assert(std::is_constructible_v<Runtime, RuntimeConfig>);

    static_assert(!std::is_copy_constructible_v<Runtime>);
    static_assert(!std::is_copy_assignable_v<Runtime>);

    static_assert(!std::is_move_constructible_v<Runtime>);
    static_assert(!std::is_move_assignable_v<Runtime>);

    static_assert(std::is_destructible_v<Runtime>);
  }

  static void test_runtime_initial_state()
  {
    Runtime runtime = make_runtime(2u, 4u);

    assert(runtime.config().workerCount == 2u);
    assert(runtime.config().budget.quantum == 4u);

    assert(runtime.worker_count() == 2u);

    assert(!runtime.started());
    assert(!runtime.running());

    assert(runtime.empty());
    assert(runtime.size() == 0u);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 0u);
  }

  static void test_start_sets_started_and_running()
  {
    Runtime runtime = make_runtime();

    assert(!runtime.started());
    assert(!runtime.running());

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    stop_runtime(runtime);
  }

  static void test_start_is_idempotent()
  {
    Runtime runtime = make_runtime();

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    runtime.start();
    runtime.start();
    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    stop_runtime(runtime);
  }

  static void test_stop_before_start_is_safe()
  {
    Runtime runtime = make_runtime();

    assert(!runtime.started());
    assert(!runtime.running());

    runtime.stop();
    runtime.stop();

    assert(!runtime.started());
    assert(!runtime.running());

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 0u);
  }

  static void test_stop_after_start_is_idempotent()
  {
    Runtime runtime = make_runtime();

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    runtime.stop();

    assert(!runtime.started());
    assert(!runtime.running());

    runtime.stop();
    runtime.stop();

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_runtime_can_restart_after_stop()
  {
    Runtime runtime = make_runtime();

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    runtime.stop();

    assert(!runtime.started());
    assert(!runtime.running());

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    stop_runtime(runtime);
  }

  static void test_next_task_id_is_monotonic()
  {
    Runtime runtime = make_runtime();

    const TaskId first = runtime.next_task_id();
    const TaskId second = runtime.next_task_id();
    const TaskId third = runtime.next_task_id();

    assert(first == 1u);
    assert(second == 2u);
    assert(third == 3u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_make_task_assigns_unique_id()
  {
    Runtime runtime = make_runtime();

    Task first = runtime.make_task(
        []()
        {
          return TaskResult::complete;
        });

    Task second = runtime.make_task(
        []()
        {
          return TaskResult::complete;
        },
        3u);

    assert(first.id == 1u);
    assert(first.affinity == 0u);
    assert(first.state == TaskState::ready);
    assert(first.valid());
    assert(first.schedulable());

    assert(second.id == 2u);
    assert(second.affinity == 3u);
    assert(second.state == TaskState::ready);
    assert(second.valid());
    assert(second.schedulable());

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 0u);
  }

  static void test_make_task_with_empty_callable_creates_invalid_task()
  {
    Runtime runtime = make_runtime();

    Task task = runtime.make_task(TaskFn{});

    assert(task.id == 1u);
    assert(task.affinity == 0u);
    assert(!task.valid());
    assert(!task.schedulable());
  }

  static void test_submit_task_before_start_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    const bool accepted = runtime.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_submit_callable_before_start_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    const bool accepted = runtime.submit(
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::complete;
        });

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);
  }

  static void test_submit_empty_callable_is_rejected()
  {
    Runtime runtime = make_runtime();

    const bool accepted = runtime.submit(TaskFn{});

    assert(!accepted);
    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);

    runtime.start();

    const bool accepted_after_start = runtime.submit(TaskFn{});

    assert(!accepted_after_start);
    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 2u);

    stop_runtime(runtime);
  }

  static void test_submit_invalid_task_while_started_is_rejected()
  {
    Runtime runtime = make_runtime();

    runtime.start();

    const bool accepted = runtime.submit(make_invalid_task());

    assert(!accepted);
    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);

    stop_runtime(runtime);
  }

  static void test_submit_running_task_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::running;

    runtime.start();

    const bool accepted = runtime.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);

    stop_runtime(runtime);
  }

  static void test_submit_completed_task_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::completed;

    runtime.start();

    const bool accepted = runtime.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);

    stop_runtime(runtime);
  }

  static void test_submit_cancelled_task_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::cancelled;

    runtime.start();

    const bool accepted = runtime.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);

    stop_runtime(runtime);
  }

  static void test_submit_complete_task_executes()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();

    const bool accepted = runtime.submit(
        make_complete_task(1u, counter));

    assert(accepted);
    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&runtime]()
        {
          return runtime.empty();
        }));

    stop_runtime(runtime);
  }

  static void test_submit_task_with_zero_id_assigns_runtime_id()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    Task task = make_complete_task(0u, counter);

    assert(task.id == 0u);

    runtime.start();

    const bool accepted = runtime.submit(std::move(task));

    assert(accepted);
    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    stop_runtime(runtime);
  }

  static void test_submit_callable_executes()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();

    const bool accepted = runtime.submit(
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::complete;
        });

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

  static void test_submit_callable_with_affinity_executes()
  {
    Runtime runtime = make_runtime(2u, 8u);

    std::atomic<int> counter{0};

    runtime.start();

    const bool accepted = runtime.submit(
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::complete;
        },
        1u);

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

  static void test_submit_many_tasks_execute()
  {
    Runtime runtime = make_runtime(4u, 8u);

    std::atomic<int> counter{0};

    runtime.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = runtime.submit(
          make_complete_task(
              static_cast<TaskId>(i + 1),
              counter));

      assert(accepted);
    }

    assert(runtime.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(runtime.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    assert(wait_until(
        [&runtime]()
        {
          return runtime.empty();
        },
        2000ms));

    stop_runtime(runtime);
  }

  static void test_submit_many_callables_execute()
  {
    Runtime runtime = make_runtime(4u, 8u);

    std::atomic<int> counter{0};

    runtime.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = runtime.submit(
          [&counter]()
          {
            counter.fetch_add(1, std::memory_order_relaxed);
            return TaskResult::complete;
          });

      assert(accepted);
    }

    assert(runtime.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(runtime.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    stop_runtime(runtime);
  }

  static void test_failed_task_is_counted_as_submitted_not_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();

    const bool accepted = runtime.submit(
        make_failed_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

  static void test_throwing_task_is_counted_as_submitted_not_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();

    const bool accepted = runtime.submit(
        make_throwing_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

  static void test_yielding_task_reschedules_until_complete()
  {
    Runtime runtime = make_runtime(1u, 4u);

    std::atomic<int> calls{0};

    Task task{
        1u,
        [&calls]()
        {
          const int current = calls.fetch_add(1, std::memory_order_relaxed) + 1;

          if (current < 3)
          {
            return TaskResult::yield;
          }

          return TaskResult::complete;
        }};

    runtime.start();

    const bool accepted = runtime.submit(std::move(task));

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) >= 3;
        }));

    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

  static void test_submit_after_stop_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();
    stop_runtime(runtime);

    const bool accepted = runtime.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);
  }

  static void test_submit_callable_after_stop_is_rejected()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();
    stop_runtime(runtime);

    const bool accepted = runtime.submit(
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::complete;
        });

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 1u);
  }

  static void test_submit_after_restart_executes()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();
    stop_runtime(runtime);

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    const bool accepted = runtime.submit(
        make_complete_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(runtime.submitted_tasks() == 1u);
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

  static void test_metrics_accumulate_across_restart()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();

    assert(runtime.submit(make_complete_task(1u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    stop_runtime(runtime);

    assert(runtime.submit(make_complete_task(2u, counter)) == false);

    assert(runtime.rejected_tasks() == 1u);

    runtime.start();

    assert(runtime.submit(make_complete_task(3u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    assert(runtime.submitted_tasks() == 2u);
    assert(runtime.rejected_tasks() == 1u);

    stop_runtime(runtime);
  }

  static void test_size_and_empty_are_safe_before_start()
  {
    Runtime runtime = make_runtime();

    assert(runtime.empty());
    assert(runtime.size() == 0u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_size_and_empty_after_completed_task()
  {
    Runtime runtime = make_runtime();

    std::atomic<int> counter{0};

    runtime.start();

    assert(runtime.submit(make_complete_task(1u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&runtime]()
        {
          return runtime.empty() && runtime.size() == 0u;
        }));

    stop_runtime(runtime);

    assert(runtime.empty());
    assert(runtime.size() == 0u);
  }

  static void test_config_and_worker_count_remain_stable_during_lifecycle()
  {
    Runtime runtime{
        RuntimeConfig{
            2u,
            BudgetConfig{9u}}};

    assert(runtime.config().workerCount == 2u);
    assert(runtime.config().budget.quantum == 9u);
    assert(runtime.worker_count() == 2u);

    runtime.start();

    assert(runtime.config().workerCount == 2u);
    assert(runtime.config().budget.quantum == 9u);
    assert(runtime.worker_count() == 2u);

    runtime.stop();

    assert(runtime.config().workerCount == 2u);
    assert(runtime.config().budget.quantum == 9u);
    assert(runtime.worker_count() == 2u);

    runtime.start();

    assert(runtime.config().workerCount == 2u);
    assert(runtime.config().budget.quantum == 9u);
    assert(runtime.worker_count() == 2u);

    runtime.stop();
  }

  static void test_next_task_id_continues_after_make_task_and_submit_callable()
  {
    Runtime runtime = make_runtime();

    Task first = runtime.make_task(
        []()
        {
          return TaskResult::complete;
        });

    assert(first.id == 1u);

    const TaskId second = runtime.next_task_id();

    assert(second == 2u);

    std::atomic<int> counter{0};

    runtime.start();

    const bool accepted = runtime.submit(
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::complete;
        });

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    const TaskId after_submit_callable = runtime.next_task_id();

    assert(after_submit_callable == 4u);

    stop_runtime(runtime);
  }

  static void test_destructor_stops_running_runtime()
  {
    std::atomic<int> counter{0};

    {
      Runtime runtime = make_runtime(2u, 8u);

      runtime.start();

      assert(runtime.submit(
          [&counter]()
          {
            counter.fetch_add(1, std::memory_order_relaxed);
            return TaskResult::complete;
          }));

      assert(wait_until(
          [&counter]()
          {
            return counter.load(std::memory_order_relaxed) == 1;
          }));
    }

    assert(counter.load(std::memory_order_relaxed) == 1);
  }

  static void test_destructor_handles_never_started_runtime()
  {
    Runtime runtime = make_runtime();

    assert(!runtime.started());
    assert(!runtime.running());
    assert(runtime.empty());
  }

  static void test_many_tasks_with_affinity_complete()
  {
    Runtime runtime = make_runtime(4u, 8u);

    std::atomic<int> counter{0};

    runtime.start();

    constexpr int count = 200;

    for (int i = 0; i < count; ++i)
    {
      const std::uint32_t affinity =
          static_cast<std::uint32_t>((i % 4) + 1);

      assert(runtime.submit(
          make_complete_task(
              static_cast<TaskId>(i + 1),
              counter,
              affinity)));
    }

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    assert(runtime.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(runtime.rejected_tasks() == 0u);

    stop_runtime(runtime);
  }

} // namespace

int main()
{
  test_runtime_type_traits();
  test_runtime_initial_state();

  test_start_sets_started_and_running();
  test_start_is_idempotent();

  test_stop_before_start_is_safe();
  test_stop_after_start_is_idempotent();

  test_runtime_can_restart_after_stop();

  test_next_task_id_is_monotonic();

  test_make_task_assigns_unique_id();
  test_make_task_with_empty_callable_creates_invalid_task();

  test_submit_task_before_start_is_rejected();
  test_submit_callable_before_start_is_rejected();
  test_submit_empty_callable_is_rejected();

  test_submit_invalid_task_while_started_is_rejected();
  test_submit_running_task_is_rejected();
  test_submit_completed_task_is_rejected();
  test_submit_cancelled_task_is_rejected();

  test_submit_complete_task_executes();
  test_submit_task_with_zero_id_assigns_runtime_id();
  test_submit_callable_executes();
  test_submit_callable_with_affinity_executes();

  test_submit_many_tasks_execute();
  test_submit_many_callables_execute();

  test_failed_task_is_counted_as_submitted_not_rejected();
  test_throwing_task_is_counted_as_submitted_not_rejected();

  test_yielding_task_reschedules_until_complete();

  test_submit_after_stop_is_rejected();
  test_submit_callable_after_stop_is_rejected();
  test_submit_after_restart_executes();

  test_metrics_accumulate_across_restart();

  test_size_and_empty_are_safe_before_start();
  test_size_and_empty_after_completed_task();

  test_config_and_worker_count_remain_stable_during_lifecycle();
  test_next_task_id_continues_after_make_task_and_submit_callable();

  test_destructor_stops_running_runtime();
  test_destructor_handles_never_started_runtime();

  test_many_tasks_with_affinity_complete();

  return 0;
}
