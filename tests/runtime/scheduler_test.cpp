/**
 *
 * @file scheduler_test.cpp
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
#include <vix/runtime/Scheduler.hpp>
#include <vix/runtime/Task.hpp>
#include <vix/runtime/Worker.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Scheduler = vix::runtime::Scheduler;
  using Task = vix::runtime::Task;
  using TaskFn = vix::runtime::TaskFn;
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
          throw std::runtime_error("scheduler task failed");
        },
        affinity};
  }

  static Task make_invalid_task(TaskId id = 0)
  {
    return Task{
        id,
        TaskFn{}};
  }

  static void stop_scheduler(Scheduler &scheduler)
  {
    scheduler.stop();
    assert(!scheduler.running());
  }

  static void test_scheduler_type_traits()
  {
    static_assert(!std::is_default_constructible_v<Scheduler>);

    static_assert(std::is_constructible_v<Scheduler, std::uint32_t>);
    static_assert(std::is_constructible_v<Scheduler, std::uint32_t, BudgetConfig>);

    static_assert(!std::is_copy_constructible_v<Scheduler>);
    static_assert(!std::is_copy_assignable_v<Scheduler>);

    static_assert(!std::is_move_constructible_v<Scheduler>);
    static_assert(!std::is_move_assignable_v<Scheduler>);

    static_assert(std::is_destructible_v<Scheduler>);
  }

  static void test_zero_worker_count_is_normalized_to_one()
  {
    Scheduler scheduler{0u, BudgetConfig{4u}};

    assert(!scheduler.running());
    assert(scheduler.worker_count() == 1u);
    assert(scheduler.empty());
    assert(scheduler.size() == 0u);

    assert(scheduler.worker_at(0u) != nullptr);
    assert(scheduler.worker_at(1u) == nullptr);

    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 0u);
    assert(scheduler.stolen_tasks() == 0u);
  }

  static void test_worker_count_matches_constructor()
  {
    Scheduler scheduler{4u, BudgetConfig{4u}};

    assert(!scheduler.running());
    assert(scheduler.worker_count() == 4u);

    assert(scheduler.worker_at(0u) != nullptr);
    assert(scheduler.worker_at(1u) != nullptr);
    assert(scheduler.worker_at(2u) != nullptr);
    assert(scheduler.worker_at(3u) != nullptr);
    assert(scheduler.worker_at(4u) == nullptr);

    for (std::size_t i = 0; i < scheduler.worker_count(); ++i)
    {
      const Worker *worker = scheduler.worker_at(i);

      assert(worker != nullptr);
      assert(worker->id() == static_cast<std::uint32_t>(i));
      assert(!worker->running());
      assert(worker->empty());
    }
  }

  static void test_start_and_stop_are_idempotent()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    assert(!scheduler.running());

    scheduler.start();

    assert(scheduler.running());

    scheduler.start();
    scheduler.start();

    assert(scheduler.running());

    stop_scheduler(scheduler);

    scheduler.stop();
    scheduler.stop();

    assert(!scheduler.running());
  }

  static void test_workers_run_after_scheduler_start()
  {
    Scheduler scheduler{3u, BudgetConfig{4u}};

    scheduler.start();

    assert(scheduler.running());

    for (std::size_t i = 0; i < scheduler.worker_count(); ++i)
    {
      Worker *worker = scheduler.worker_at(i);

      assert(worker != nullptr);
      assert(worker->running());
    }

    stop_scheduler(scheduler);

    for (std::size_t i = 0; i < scheduler.worker_count(); ++i)
    {
      Worker *worker = scheduler.worker_at(i);

      assert(worker != nullptr);
      assert(!worker->running());
    }
  }

  static void test_submit_before_start_is_rejected()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    const bool accepted = scheduler.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);
    assert(scheduler.empty());
    assert(scheduler.size() == 0u);
  }

  static void test_submit_invalid_task_before_start_is_rejected_once()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    const bool accepted = scheduler.submit(make_invalid_task());

    assert(!accepted);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);
    assert(scheduler.empty());
  }

  static void test_submit_complete_task_executes()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    const bool accepted = scheduler.submit(
        make_complete_task(1u, counter));

    assert(accepted);
    assert(scheduler.submitted_tasks() == 1u);
    assert(scheduler.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&scheduler]()
        {
          return scheduler.empty();
        }));

    stop_scheduler(scheduler);
  }

  static void test_submit_many_tasks_execute()
  {
    Scheduler scheduler{4u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = scheduler.submit(
          make_complete_task(
              static_cast<TaskId>(i + 1),
              counter));

      assert(accepted);
    }

    assert(scheduler.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(scheduler.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        }));

    assert(wait_until(
        [&scheduler]()
        {
          return scheduler.empty();
        }));

    stop_scheduler(scheduler);
  }

  static void test_submit_with_affinity_executes()
  {
    Scheduler scheduler{3u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    const bool accepted = scheduler.submit(
        make_complete_task(
            1u,
            counter,
            2u));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(scheduler.submitted_tasks() == 1u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_submit_with_large_affinity_executes()
  {
    Scheduler scheduler{3u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    const bool accepted = scheduler.submit(
        make_complete_task(
            1u,
            counter,
            999u));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(scheduler.submitted_tasks() == 1u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_submit_rejects_invalid_task_while_running()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    scheduler.start();

    const bool accepted = scheduler.submit(make_invalid_task());

    assert(!accepted);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);

    stop_scheduler(scheduler);
  }

  static void test_submit_rejects_running_task()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::running;

    scheduler.start();

    const bool accepted = scheduler.submit(std::move(task));

    assert(!accepted);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);
    assert(counter.load(std::memory_order_relaxed) == 0);

    stop_scheduler(scheduler);
  }

  static void test_submit_rejects_completed_task()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::completed;

    scheduler.start();

    const bool accepted = scheduler.submit(std::move(task));

    assert(!accepted);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);
    assert(counter.load(std::memory_order_relaxed) == 0);

    stop_scheduler(scheduler);
  }

  static void test_submit_rejects_cancelled_task()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    Task task = make_complete_task(1u, counter);
    task.state = TaskState::cancelled;

    scheduler.start();

    const bool accepted = scheduler.submit(std::move(task));

    assert(!accepted);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);
    assert(counter.load(std::memory_order_relaxed) == 0);

    stop_scheduler(scheduler);
  }

  static void test_submit_failed_result_task_executes_and_worker_records_failure()
  {
    Scheduler scheduler{1u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    const bool accepted = scheduler.submit(
        make_failed_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    Worker *worker = scheduler.worker_at(0u);

    assert(worker != nullptr);

    assert(wait_until(
        [worker]()
        {
          return worker->failed_tasks() >= 1u;
        }));

    assert(scheduler.submitted_tasks() == 1u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_submit_throwing_task_executes_and_worker_records_failure()
  {
    Scheduler scheduler{1u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    const bool accepted = scheduler.submit(
        make_throwing_task(1u, counter));

    assert(accepted);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    Worker *worker = scheduler.worker_at(0u);

    assert(worker != nullptr);

    assert(wait_until(
        [worker]()
        {
          return worker->failed_tasks() >= 1u;
        }));

    assert(scheduler.submitted_tasks() == 1u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_submit_batch_before_start_rejects_all()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter));
    tasks.push_back(make_complete_task(2u, counter));
    tasks.push_back(make_complete_task(3u, counter));

    const std::size_t accepted = scheduler.submit_batch(std::move(tasks));

    assert(accepted == 0u);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 3u);
    assert(scheduler.empty());
  }

  static void test_submit_batch_empty_before_start()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::vector<Task> tasks;

    const std::size_t accepted = scheduler.submit_batch(std::move(tasks));

    assert(accepted == 0u);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 0u);
    assert(scheduler.empty());
  }

  static void test_submit_batch_accepts_all_valid_tasks()
  {
    Scheduler scheduler{4u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    std::vector<Task> tasks;

    for (TaskId id = 1u; id <= 20u; ++id)
    {
      tasks.push_back(make_complete_task(id, counter));
    }

    scheduler.start();

    const std::size_t accepted = scheduler.submit_batch(std::move(tasks));

    assert(accepted == 20u);
    assert(scheduler.submitted_tasks() == 20u);
    assert(scheduler.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 20;
        }));

    assert(wait_until(
        [&scheduler]()
        {
          return scheduler.empty();
        }));

    stop_scheduler(scheduler);
  }

  static void test_submit_batch_accepts_only_schedulable_tasks()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    Task running = make_complete_task(2u, counter);
    running.state = TaskState::running;

    Task completed = make_complete_task(4u, counter);
    completed.state = TaskState::completed;

    Task cancelled = make_complete_task(6u, counter);
    cancelled.state = TaskState::cancelled;

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter));
    tasks.push_back(std::move(running));
    tasks.push_back(make_complete_task(3u, counter));
    tasks.push_back(std::move(completed));
    tasks.push_back(make_invalid_task(5u));
    tasks.push_back(std::move(cancelled));

    scheduler.start();

    const std::size_t accepted = scheduler.submit_batch(std::move(tasks));

    assert(accepted == 2u);
    assert(scheduler.submitted_tasks() == 2u);
    assert(scheduler.rejected_tasks() == 4u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    stop_scheduler(scheduler);
  }

  static void test_submit_batch_with_affinity_executes()
  {
    Scheduler scheduler{3u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter, 1u));
    tasks.push_back(make_complete_task(2u, counter, 2u));
    tasks.push_back(make_complete_task(3u, counter, 99u));

    scheduler.start();

    const std::size_t accepted = scheduler.submit_batch(std::move(tasks));

    assert(accepted == 3u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 3;
        }));

    assert(scheduler.submitted_tasks() == 3u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_worker_at_returns_nullptr_for_out_of_range()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    assert(scheduler.worker_at(0u) != nullptr);
    assert(scheduler.worker_at(1u) != nullptr);
    assert(scheduler.worker_at(2u) == nullptr);
    assert(scheduler.worker_at(100u) == nullptr);
  }

  static void test_const_worker_at_returns_nullptr_for_out_of_range()
  {
    const Scheduler scheduler{2u, BudgetConfig{4u}};

    assert(scheduler.worker_at(0u) != nullptr);
    assert(scheduler.worker_at(1u) != nullptr);
    assert(scheduler.worker_at(2u) == nullptr);
    assert(scheduler.worker_at(100u) == nullptr);
  }

  static void test_try_steal_single_worker_returns_nullopt()
  {
    Scheduler scheduler{1u, BudgetConfig{4u}};

    auto stolen = scheduler.try_steal(0u);

    assert(!stolen.has_value());
    assert(scheduler.stolen_tasks() == 0u);
  }

  static void test_try_steal_with_no_available_work_returns_nullopt()
  {
    Scheduler scheduler{3u, BudgetConfig{4u}};

    auto first = scheduler.try_steal(0u);
    auto second = scheduler.try_steal(1u);
    auto third = scheduler.try_steal(999u);

    assert(!first.has_value());
    assert(!second.has_value());
    assert(!third.has_value());
    assert(scheduler.stolen_tasks() == 0u);
  }

  static void test_size_and_empty_before_and_after_work()
  {
    Scheduler scheduler{1u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    assert(scheduler.empty());
    assert(scheduler.size() == 0u);

    scheduler.start();

    assert(scheduler.submit(make_complete_task(1u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(wait_until(
        [&scheduler]()
        {
          return scheduler.empty() && scheduler.size() == 0u;
        }));

    stop_scheduler(scheduler);

    assert(scheduler.empty());
    assert(scheduler.size() == 0u);
  }

  static void test_scheduler_can_restart_after_stop()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();
    stop_scheduler(scheduler);

    assert(!scheduler.running());

    scheduler.start();

    assert(scheduler.running());

    assert(scheduler.submit(make_complete_task(1u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    assert(scheduler.submitted_tasks() == 1u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_submit_after_stop_is_rejected()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();
    stop_scheduler(scheduler);

    const bool accepted = scheduler.submit(
        make_complete_task(1u, counter));

    assert(!accepted);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 1u);
  }

  static void test_submit_batch_after_stop_is_rejected()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();
    stop_scheduler(scheduler);

    std::vector<Task> tasks;
    tasks.push_back(make_complete_task(1u, counter));
    tasks.push_back(make_complete_task(2u, counter));

    const std::size_t accepted = scheduler.submit_batch(std::move(tasks));

    assert(accepted == 0u);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(scheduler.submitted_tasks() == 0u);
    assert(scheduler.rejected_tasks() == 2u);
  }

  static void test_metrics_accumulate_across_restart()
  {
    Scheduler scheduler{2u, BudgetConfig{4u}};

    std::atomic<int> counter{0};

    scheduler.start();

    assert(scheduler.submit(make_complete_task(1u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    stop_scheduler(scheduler);

    scheduler.start();

    assert(scheduler.submit(make_complete_task(2u, counter)));

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    assert(scheduler.submitted_tasks() == 2u);
    assert(scheduler.rejected_tasks() == 0u);

    stop_scheduler(scheduler);
  }

  static void test_destructor_stops_running_scheduler()
  {
    std::atomic<int> counter{0};

    {
      Scheduler scheduler{2u, BudgetConfig{4u}};

      scheduler.start();

      assert(scheduler.submit(make_complete_task(1u, counter)));

      assert(wait_until(
          [&counter]()
          {
            return counter.load(std::memory_order_relaxed) == 1;
          }));
    }

    assert(counter.load(std::memory_order_relaxed) == 1);
  }

  static void test_many_tasks_across_workers_complete()
  {
    Scheduler scheduler{4u, BudgetConfig{8u}};

    std::atomic<int> counter{0};

    scheduler.start();

    constexpr int count = 500;

    for (int i = 0; i < count; ++i)
    {
      assert(scheduler.submit(
          make_complete_task(
              static_cast<TaskId>(i + 1),
              counter)));
    }

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    assert(scheduler.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(scheduler.rejected_tasks() == 0u);

    assert(wait_until(
        [&scheduler]()
        {
          return scheduler.empty();
        },
        2000ms));

    stop_scheduler(scheduler);
  }

} // namespace

int main()
{
  test_scheduler_type_traits();

  test_zero_worker_count_is_normalized_to_one();
  test_worker_count_matches_constructor();

  test_start_and_stop_are_idempotent();
  test_workers_run_after_scheduler_start();

  test_submit_before_start_is_rejected();
  test_submit_invalid_task_before_start_is_rejected_once();

  test_submit_complete_task_executes();
  test_submit_many_tasks_execute();

  test_submit_with_affinity_executes();
  test_submit_with_large_affinity_executes();

  test_submit_rejects_invalid_task_while_running();
  test_submit_rejects_running_task();
  test_submit_rejects_completed_task();
  test_submit_rejects_cancelled_task();

  test_submit_failed_result_task_executes_and_worker_records_failure();
  test_submit_throwing_task_executes_and_worker_records_failure();

  test_submit_batch_before_start_rejects_all();
  test_submit_batch_empty_before_start();
  test_submit_batch_accepts_all_valid_tasks();
  test_submit_batch_accepts_only_schedulable_tasks();
  test_submit_batch_with_affinity_executes();

  test_worker_at_returns_nullptr_for_out_of_range();
  test_const_worker_at_returns_nullptr_for_out_of_range();

  test_try_steal_single_worker_returns_nullopt();
  test_try_steal_with_no_available_work_returns_nullopt();

  test_size_and_empty_before_and_after_work();

  test_scheduler_can_restart_after_stop();

  test_submit_after_stop_is_rejected();
  test_submit_batch_after_stop_is_rejected();

  test_metrics_accumulate_across_restart();

  test_destructor_stops_running_scheduler();

  test_many_tasks_across_workers_complete();

  return 0;
}
