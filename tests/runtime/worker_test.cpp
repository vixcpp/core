/**
 *
 * @file worker_test.cpp
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
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Task.hpp>
#include <vix/runtime/Worker.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Task = vix::runtime::Task;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;
  using Worker = vix::runtime::Worker;

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
      std::atomic<int> &counter)
  {
    return Task{
        id,
        [&counter]()
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::failed;
        }};
  }

  static Task make_throwing_task(
      TaskId id,
      std::atomic<int> &counter)
  {
    return Task{
        id,
        [&counter]() -> TaskResult
        {
          counter.fetch_add(1, std::memory_order_relaxed);
          throw std::runtime_error("worker task failed");
        }};
  }

  static void stop_and_join(Worker &worker)
  {
    worker.stop();
    worker.join();
  }

  static void test_worker_type_traits()
  {
    static_assert(!std::is_default_constructible_v<Worker>);

    static_assert(std::is_constructible_v<Worker, std::uint32_t>);
    static_assert(std::is_constructible_v<Worker, std::uint32_t, BudgetConfig>);

    static_assert(!std::is_copy_constructible_v<Worker>);
    static_assert(!std::is_copy_assignable_v<Worker>);

    static_assert(!std::is_move_constructible_v<Worker>);
    static_assert(!std::is_move_assignable_v<Worker>);

    static_assert(std::is_destructible_v<Worker>);
  }

  static void test_worker_initial_state()
  {
    Worker worker{7u, BudgetConfig{4u}};

    assert(worker.id() == 7u);
    assert(worker.running() == false);
    assert(worker.empty());
    assert(worker.size() == 0u);

    assert(worker.executed_tasks() == 0u);
    assert(worker.yielded_tasks() == 0u);
    assert(worker.failed_tasks() == 0u);
    assert(worker.stolen_tasks() == 0u);
    assert(worker.idle_cycles() == 0u);

    assert(!worker.try_steal().has_value());
  }

  static void test_submit_before_start_is_rejected()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    const bool accepted = worker.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(worker.empty());
    assert(worker.size() == 0u);
    assert(worker.executed_tasks() == 0u);
  }

  static void test_submit_batch_before_start_is_rejected()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter));
    tasks.push_back(make_complete_task(2u, counter));

    const std::size_t accepted = worker.submit_batch(std::move(tasks));

    assert(accepted == 0u);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(worker.empty());
    assert(worker.size() == 0u);
  }

  static void test_start_stop_join_are_idempotent()
  {
    Worker worker{0u};

    assert(!worker.running());

    worker.start();

    assert(worker.running());

    worker.start();
    worker.start();

    assert(worker.running());

    worker.stop();

    assert(!worker.running());

    worker.stop();
    worker.stop();

    assert(!worker.running());

    worker.join();
    worker.join();
  }

  static void test_complete_task_executes()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();

    assert(worker.running());

    const bool accepted = worker.submit(
        make_complete_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.executed_tasks() >= 1u;
        }));

    assert(worker.failed_tasks() == 0u);

    stop_and_join(worker);

    assert(!worker.running());
  }

  static void test_multiple_complete_tasks_execute()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();

    constexpr int count = 20;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = worker.submit(
          make_complete_task(
              static_cast<TaskId>(i + 1),
              counter));

      assert(accepted);
    }

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.executed_tasks() >= static_cast<std::uint64_t>(count);
        }));

    assert(worker.failed_tasks() == 0u);

    stop_and_join(worker);
  }

  static void test_submit_batch_executes_all_tasks()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();

    std::vector<Task> tasks;

    for (TaskId id = 1u; id <= 10u; ++id)
    {
      tasks.push_back(make_complete_task(id, counter));
    }

    const std::size_t accepted = worker.submit_batch(std::move(tasks));

    assert(accepted == 10u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 10;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.executed_tasks() >= 10u;
        }));

    stop_and_join(worker);
  }

  static void test_submit_batch_rejects_invalid_tasks_through_queue()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();

    Task running = make_complete_task(2u, counter);
    running.state = TaskState::running;

    Task completed = make_complete_task(4u, counter);
    completed.state = TaskState::completed;

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter));
    tasks.push_back(std::move(running));
    tasks.push_back(make_complete_task(3u, counter));
    tasks.push_back(std::move(completed));
    tasks.push_back(Task{});

    const std::size_t accepted = worker.submit_batch(std::move(tasks));

    assert(accepted == 2u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    stop_and_join(worker);
  }

  static void test_submit_rejects_invalid_task_while_running()
  {
    Worker worker{0u};

    worker.start();

    Task invalid;

    const bool accepted = worker.submit(std::move(invalid));

    assert(!accepted);
    assert(worker.executed_tasks() == 0u);

    stop_and_join(worker);
  }

  static void test_failed_task_updates_failed_metric()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();

    const bool accepted = worker.submit(
        make_failed_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.failed_tasks() >= 1u;
        }));

    assert(worker.executed_tasks() >= 1u);

    stop_and_join(worker);
  }

  static void test_throwing_task_updates_failed_metric()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();

    const bool accepted = worker.submit(
        make_throwing_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.failed_tasks() >= 1u;
        }));

    assert(worker.executed_tasks() >= 1u);

    stop_and_join(worker);
  }

  static void test_yielding_task_is_rescheduled_until_complete()
  {
    Worker worker{0u, BudgetConfig{4u}};

    std::atomic<int> calls{0};

    Task task{
        1u,
        [&calls]()
        {
          const int n = calls.fetch_add(1, std::memory_order_relaxed) + 1;

          if (n < 3)
          {
            return TaskResult::yield;
          }

          return TaskResult::complete;
        }};

    worker.start();

    const bool accepted = worker.submit(std::move(task));

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) >= 3;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.executed_tasks() >= 3u;
        }));

    assert(worker.yielded_tasks() >= 2u);
    assert(worker.failed_tasks() == 0u);

    stop_and_join(worker);
  }

  static void test_yielding_task_stops_cleanly_when_worker_stops()
  {
    Worker worker{0u, BudgetConfig{1u}};

    std::atomic<int> calls{0};

    Task task{
        1u,
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
          return TaskResult::yield;
        }};

    worker.start();

    const bool accepted = worker.submit(std::move(task));

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) >= 1;
        }));

    worker.stop();
    worker.join();

    assert(!worker.running());
    assert(worker.executed_tasks() >= 1u);
    assert(worker.yielded_tasks() >= 1u);
  }

  static void test_worker_can_restart_after_stop_and_join()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();
    worker.stop();
    worker.join();

    assert(!worker.running());

    worker.start();

    assert(worker.running());

    const bool accepted = worker.submit(
        make_complete_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    stop_and_join(worker);
  }

  static void test_submit_after_stop_is_rejected()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();
    worker.stop();
    worker.join();

    assert(!worker.running());

    const bool accepted = worker.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);
  }

  static void test_submit_batch_after_stop_is_rejected()
  {
    Worker worker{0u};

    std::atomic<int> counter{0};

    worker.start();
    worker.stop();
    worker.join();

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter));
    tasks.push_back(make_complete_task(2u, counter));

    const std::size_t accepted = worker.submit_batch(std::move(tasks));

    assert(accepted == 0u);
    assert(counter.load(std::memory_order_relaxed) == 0);
  }

  static void test_steal_callback_can_provide_work()
  {
    Worker worker{3u};

    std::atomic<int> counter{0};
    std::atomic<int> callback_calls{0};
    std::atomic<bool> provided{false};

    worker.set_steal_callback(
        [&counter, &callback_calls, &provided](std::uint32_t worker_id) -> std::optional<Task>
        {
          assert(worker_id == 3u);

          callback_calls.fetch_add(1, std::memory_order_relaxed);

          bool expected = false;
          if (!provided.compare_exchange_strong(expected, true))
          {
            return std::nullopt;
          }

          return make_complete_task(99u, counter);
        });

    worker.start();

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.stolen_tasks() >= 1u;
        }));

    assert(callback_calls.load(std::memory_order_relaxed) >= 1);
    assert(worker.executed_tasks() >= 1u);

    stop_and_join(worker);
  }

  static void test_empty_steal_callback_does_not_crash_worker()
  {
    Worker worker{0u};

    std::atomic<int> callback_calls{0};

    worker.set_steal_callback(
        [&callback_calls](std::uint32_t) -> std::optional<Task>
        {
          callback_calls.fetch_add(1, std::memory_order_relaxed);
          return std::nullopt;
        });

    worker.start();

    assert(wait_until(
        [&callback_calls]()
        {
          return callback_calls.load(std::memory_order_relaxed) >= 1;
        }));

    assert(worker.executed_tasks() == 0u);
    assert(worker.failed_tasks() == 0u);

    stop_and_join(worker);
  }

  static void test_try_steal_on_empty_worker_returns_nullopt()
  {
    Worker worker{0u};

    assert(!worker.running());
    assert(worker.empty());
    assert(worker.size() == 0u);

    auto task = worker.try_steal();

    assert(!task.has_value());
  }

  static void test_metrics_start_at_zero_and_increase()
  {
    Worker worker{0u};

    std::atomic<int> complete_count{0};
    std::atomic<int> failed_count{0};

    assert(worker.executed_tasks() == 0u);
    assert(worker.yielded_tasks() == 0u);
    assert(worker.failed_tasks() == 0u);
    assert(worker.stolen_tasks() == 0u);

    worker.start();

    assert(worker.submit(make_complete_task(1u, complete_count)));
    assert(worker.submit(make_failed_task(2u, failed_count)));

    assert(wait_until(
        [&complete_count, &failed_count]()
        {
          return complete_count.load(std::memory_order_relaxed) == 1 &&
                 failed_count.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&worker]()
        {
          return worker.executed_tasks() >= 2u;
        }));

    assert(worker.failed_tasks() >= 1u);

    stop_and_join(worker);
  }

  static void test_destructor_stops_running_worker()
  {
    std::atomic<int> calls{0};

    {
      Worker worker{0u};

      Task task{
          1u,
          [&calls]()
          {
            calls.fetch_add(1, std::memory_order_relaxed);
            return TaskResult::yield;
          }};

      worker.start();

      assert(worker.submit(std::move(task)));

      assert(wait_until(
          [&calls]()
          {
            return calls.load(std::memory_order_relaxed) >= 1;
          }));
    }

    assert(calls.load(std::memory_order_relaxed) >= 1);
  }

} // namespace

int main()
{
  test_worker_type_traits();
  test_worker_initial_state();

  test_submit_before_start_is_rejected();
  test_submit_batch_before_start_is_rejected();

  test_start_stop_join_are_idempotent();

  test_complete_task_executes();
  test_multiple_complete_tasks_execute();
  test_submit_batch_executes_all_tasks();
  test_submit_batch_rejects_invalid_tasks_through_queue();

  test_submit_rejects_invalid_task_while_running();

  test_failed_task_updates_failed_metric();
  test_throwing_task_updates_failed_metric();

  test_yielding_task_is_rescheduled_until_complete();
  test_yielding_task_stops_cleanly_when_worker_stops();

  test_worker_can_restart_after_stop_and_join();

  test_submit_after_stop_is_rejected();
  test_submit_batch_after_stop_is_rejected();

  test_steal_callback_can_provide_work();
  test_empty_steal_callback_does_not_crash_worker();

  test_try_steal_on_empty_worker_returns_nullopt();

  test_metrics_start_at_zero_and_increase();

  test_destructor_stops_running_worker();

  return 0;
}
