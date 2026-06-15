/**
 *
 * @file runtime_executor_submit_test.cpp
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
#include <utility>
#include <vector>

#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/runtime/Task.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Metrics = vix::executor::Metrics;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;
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

  static RuntimeExecutor make_executor(
      std::uint32_t workers = 1u,
      std::uint32_t quantum = 8u)
  {
    return RuntimeExecutor{
        RuntimeConfig{
            workers,
            BudgetConfig{quantum}}};
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

  static Task make_yielding_then_complete_task(
      TaskId id,
      std::atomic<int> &calls,
      int complete_after,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&calls, complete_after]()
        {
          const int n = calls.fetch_add(1, std::memory_order_relaxed) + 1;

          if (n < complete_after)
          {
            return TaskResult::yield;
          }

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
          throw std::runtime_error("executor submitted task failed");
        },
        affinity};
  }

  static Task make_invalid_task(TaskId id = 0)
  {
    return Task{
        id,
        TaskFn{}};
  }

  static void assert_empty_metrics(const RuntimeExecutor &executor)
  {
    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void stop_executor(RuntimeExecutor &executor)
  {
    executor.stop();

    assert(executor.started() == false);
    assert(executor.running() == false);
    assert(executor.accepting() == false);
  }

  static void test_submit_task_before_start_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    const bool accepted = executor.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);
  }

  static void test_submit_taskfn_before_start_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    TaskFn fn =
        [&counter]()
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.submit(std::move(fn));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);
  }

  static void test_submit_empty_taskfn_is_rejected_before_start()
  {
    RuntimeExecutor executor = make_executor();

    const bool accepted = executor.submit(TaskFn{});

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);
  }

  static void test_submit_empty_taskfn_is_rejected_after_start()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    const bool accepted = executor.submit(TaskFn{});

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_valid_task_after_start_executes()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.submit(
        make_complete_task(1u, counter));

    assert(accepted);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(counter.load(std::memory_order_relaxed) == 1);
    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_submit_valid_taskfn_after_start_executes()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    TaskFn fn =
        [&counter]()
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.submit(std::move(fn));

    assert(accepted);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_submit_taskfn_with_affinity_executes()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> counter{0};

    executor.start();

    TaskFn fn =
        [&counter]()
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.submit(std::move(fn), 1u);

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_task_with_affinity_executes()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.submit(
        make_complete_task(1u, counter, 1u));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_task_with_large_affinity_executes()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.submit(
        make_complete_task(1u, counter, 999u));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_invalid_task_after_start_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    const bool accepted = executor.submit(make_invalid_task());

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_running_task_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::running;

    executor.start();

    const bool accepted = executor.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_submit_completed_task_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::completed;

    executor.start();

    const bool accepted = executor.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_submit_failed_state_task_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::failed;

    executor.start();

    const bool accepted = executor.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_submit_cancelled_task_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::cancelled;

    executor.start();

    const bool accepted = executor.submit(std::move(task));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_submit_yielded_task_is_accepted_and_normalized_by_runtime()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::yielded;

    executor.start();

    const bool accepted = executor.submit(std::move(task));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_yielding_task_reschedules_until_complete()
  {
    RuntimeExecutor executor = make_executor(1u, 4u);

    std::atomic<int> calls{0};

    executor.start();

    const bool accepted = executor.submit(
        make_yielding_then_complete_task(
            1u,
            calls,
            3));

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) >= 3;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_failed_result_task_is_accepted_not_executor_failed()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.submit(
        make_failed_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    /*
     * RuntimeExecutor::failed_tasks() tracks failures observed by post().
     * Raw runtime submit() failures are handled by the runtime worker.
     */
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_throwing_task_is_accepted_not_executor_failed()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.submit(
        make_throwing_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_taskfn_returning_failed_is_accepted_not_executor_failed()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    TaskFn fn =
        [&counter]()
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::failed;
    };

    const bool accepted = executor.submit(std::move(fn));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_taskfn_throwing_is_accepted_not_executor_failed()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    TaskFn fn =
        [&counter]() -> TaskResult
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      throw std::runtime_error("raw taskfn failed");
    };

    const bool accepted = executor.submit(std::move(fn));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_many_tasks_execute()
  {
    RuntimeExecutor executor = make_executor(4u, 8u);

    std::atomic<int> counter{0};

    executor.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = executor.submit(
          make_complete_task(
              static_cast<TaskId>(i + 1),
              counter));

      assert(accepted);
    }

    assert(executor.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(executor.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    stop_executor(executor);
  }

  static void test_submit_many_taskfns_execute()
  {
    RuntimeExecutor executor = make_executor(4u, 8u);

    std::atomic<int> counter{0};

    executor.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      TaskFn fn =
          [&counter]()
      {
        counter.fetch_add(1, std::memory_order_relaxed);
        return TaskResult::complete;
      };

      const bool accepted = executor.submit(std::move(fn));

      assert(accepted);
    }

    assert(executor.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(executor.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    stop_executor(executor);
  }

  static void test_submit_mixed_valid_and_invalid_tasks_updates_counters()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> counter{0};

    executor.start();

    assert(executor.submit(make_complete_task(1u, counter)) == true);
    assert(executor.submit(make_invalid_task()) == false);

    Task running = make_complete_task(2u, counter);
    running.state = TaskState::running;

    assert(executor.submit(std::move(running)) == false);

    assert(executor.submit(make_complete_task(3u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 2u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_submit_after_stop_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();
    stop_executor(executor);

    const bool accepted = executor.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
  }

  static void test_submit_taskfn_after_stop_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();
    stop_executor(executor);

    TaskFn fn =
        [&counter]()
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.submit(std::move(fn));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
  }

  static void test_submit_after_restart_executes()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();
    stop_executor(executor);

    assert(executor.submit(make_complete_task(1u, counter)) == false);
    assert(executor.rejected_tasks() == 1u);

    executor.start();

    assert(executor.submit(make_complete_task(2u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_submit_preserves_counters_across_restart()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    assert(executor.submit(make_complete_task(1u, counter)) == false);

    executor.start();

    assert(executor.submit(make_complete_task(2u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    stop_executor(executor);

    assert(executor.submit(make_complete_task(3u, counter)) == false);

    executor.start();

    assert(executor.submit(make_complete_task(4u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 2u);

    stop_executor(executor);
  }

  static void test_submit_does_not_increment_fast_http_counter()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> counter{0};

    executor.start();

    assert(executor.submit(make_complete_task(1u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_multiple_executors_submit_independently()
  {
    RuntimeExecutor first = make_executor();
    RuntimeExecutor second = make_executor();

    std::atomic<int> first_counter{0};
    std::atomic<int> second_counter{0};

    first.start();

    assert(first.submit(make_complete_task(1u, first_counter)) == true);
    assert(second.submit(make_complete_task(1u, second_counter)) == false);

    assert(wait_until(
        [&first_counter]()
        {
          return first_counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(first.submitted_tasks() == 1u);
    assert(first.rejected_tasks() == 0u);

    assert(second.submitted_tasks() == 0u);
    assert(second.rejected_tasks() == 1u);

    assert(second_counter.load(std::memory_order_relaxed) == 0);

    first.stop();
    second.stop();
  }

} // namespace

int main()
{
  test_submit_task_before_start_is_rejected();
  test_submit_taskfn_before_start_is_rejected();

  test_submit_empty_taskfn_is_rejected_before_start();
  test_submit_empty_taskfn_is_rejected_after_start();

  test_submit_valid_task_after_start_executes();
  test_submit_valid_taskfn_after_start_executes();

  test_submit_taskfn_with_affinity_executes();
  test_submit_task_with_affinity_executes();
  test_submit_task_with_large_affinity_executes();

  test_submit_invalid_task_after_start_is_rejected();
  test_submit_running_task_is_rejected();
  test_submit_completed_task_is_rejected();
  test_submit_failed_state_task_is_rejected();
  test_submit_cancelled_task_is_rejected();
  test_submit_yielded_task_is_accepted_and_normalized_by_runtime();

  test_submit_yielding_task_reschedules_until_complete();

  test_submit_failed_result_task_is_accepted_not_executor_failed();
  test_submit_throwing_task_is_accepted_not_executor_failed();
  test_submit_taskfn_returning_failed_is_accepted_not_executor_failed();
  test_submit_taskfn_throwing_is_accepted_not_executor_failed();

  test_submit_many_tasks_execute();
  test_submit_many_taskfns_execute();

  test_submit_mixed_valid_and_invalid_tasks_updates_counters();

  test_submit_after_stop_is_rejected();
  test_submit_taskfn_after_stop_is_rejected();
  test_submit_after_restart_executes();
  test_submit_preserves_counters_across_restart();

  test_submit_does_not_increment_fast_http_counter();

  test_multiple_executors_submit_independently();

  return 0;
}
