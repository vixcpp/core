/**
 *
 * @file runtime_executor_lifecycle_test.cpp
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
#include <thread>
#include <type_traits>
#include <utility>

#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/runtime/Task.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Metrics = vix::executor::Metrics;
  using Runtime = vix::runtime::Runtime;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;
  using Task = vix::runtime::Task;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;

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

  static void assert_stopped_state(const RuntimeExecutor &executor)
  {
    assert(executor.started() == false);
    assert(executor.running() == false);
    assert(executor.accepting() == false);
    assert(executor.runtime().started() == false);
    assert(executor.runtime().running() == false);
  }

  static void assert_started_state(const RuntimeExecutor &executor)
  {
    assert(executor.started() == true);
    assert(executor.running() == true);
    assert(executor.accepting() == true);
    assert(executor.runtime().started() == true);
    assert(executor.runtime().running() == true);
  }

  static void assert_zero_metrics(const RuntimeExecutor &executor)
  {
    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_initial_lifecycle_state()
  {
    RuntimeExecutor executor{1u};

    assert_stopped_state(executor);
    assert_zero_metrics(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    executor.stop();
  }

  static void test_start_transitions_to_started_running_accepting()
  {
    RuntimeExecutor executor{1u};

    assert_stopped_state(executor);

    executor.start();

    assert_started_state(executor);
    assert_zero_metrics(executor);

    executor.stop();

    assert_stopped_state(executor);
  }

  static void test_start_is_idempotent()
  {
    RuntimeExecutor executor{1u};

    executor.start();

    assert_started_state(executor);

    executor.start();
    executor.start();
    executor.start();

    assert_started_state(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);

    executor.stop();

    assert_stopped_state(executor);
  }

  static void test_stop_before_start_is_safe()
  {
    RuntimeExecutor executor{1u};

    assert_stopped_state(executor);

    executor.stop();
    executor.stop();
    executor.stop();

    assert_stopped_state(executor);
    assert_zero_metrics(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
  }

  static void test_stop_after_start_transitions_to_stopped()
  {
    RuntimeExecutor executor{1u};

    executor.start();

    assert_started_state(executor);

    executor.stop();

    assert_stopped_state(executor);
    assert_zero_metrics(executor);
  }

  static void test_stop_is_idempotent_after_start()
  {
    RuntimeExecutor executor{1u};

    executor.start();

    assert_started_state(executor);

    executor.stop();

    assert_stopped_state(executor);

    executor.stop();
    executor.stop();
    executor.stop();

    assert_stopped_state(executor);
    assert_zero_metrics(executor);
  }

  static void test_executor_can_restart_after_stop()
  {
    RuntimeExecutor executor{1u};

    executor.start();

    assert_started_state(executor);

    executor.stop();

    assert_stopped_state(executor);

    executor.start();

    assert_started_state(executor);

    executor.stop();

    assert_stopped_state(executor);
  }

  static void test_accepting_tracks_lifecycle()
  {
    RuntimeExecutor executor{1u};

    assert(executor.accepting() == false);

    executor.start();

    assert(executor.accepting() == true);

    executor.stop();

    assert(executor.accepting() == false);

    executor.start();

    assert(executor.accepting() == true);

    executor.stop();

    assert(executor.accepting() == false);
  }

  static void test_submit_is_rejected_before_start_and_after_stop()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    assert_stopped_state(executor);

    const bool before_start = executor.submit(
        make_complete_task(1u, counter));

    assert(before_start == false);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);

    executor.start();

    assert_started_state(executor);

    executor.stop();

    assert_stopped_state(executor);

    const bool after_stop = executor.submit(
        make_complete_task(2u, counter));

    assert(after_stop == false);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 2u);
  }

  static void test_submit_executes_after_start()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    executor.start();

    assert_started_state(executor);

    const bool accepted = executor.submit(
        make_complete_task(1u, counter));

    assert(accepted == true);
    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(counter.load(std::memory_order_relaxed) == 1);
    assert_zero_metrics(executor);

    executor.stop();

    assert_stopped_state(executor);
  }

  static void test_submit_executes_after_restart()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    executor.start();

    executor.stop();

    assert_stopped_state(executor);

    const bool rejected = executor.submit(
        make_complete_task(1u, counter));

    assert(rejected == false);
    assert(counter.load(std::memory_order_relaxed) == 0);

    executor.start();

    assert_started_state(executor);

    const bool accepted = executor.submit(
        make_complete_task(2u, counter));

    assert(accepted == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 1u);

    executor.stop();

    assert_stopped_state(executor);
  }

  static void test_wait_idle_is_safe_before_start()
  {
    RuntimeExecutor executor{1u};

    assert_stopped_state(executor);

    executor.wait_idle();
    executor.wait_idle();

    assert_stopped_state(executor);
    assert_zero_metrics(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
  }

  static void test_wait_idle_waits_for_started_work()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.post(
        [&counter]()
        {
          std::this_thread::sleep_for(10ms);
          counter.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted == true);

    executor.wait_idle();

    assert(counter.load(std::memory_order_relaxed) == 1);
    assert_zero_metrics(executor);

    executor.stop();
  }

  static void test_stop_and_wait_before_start_is_safe()
  {
    RuntimeExecutor executor{1u};

    assert_stopped_state(executor);

    executor.stop_and_wait();
    executor.stop_and_wait();

    assert_stopped_state(executor);
    assert_zero_metrics(executor);
  }

  static void test_stop_and_wait_stops_started_executor()
  {
    RuntimeExecutor executor{1u};

    executor.start();

    assert_started_state(executor);

    executor.stop_and_wait();

    assert_stopped_state(executor);
    assert_zero_metrics(executor);
  }

  static void test_stop_and_wait_waits_for_current_work()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    executor.start();

    const bool accepted = executor.post(
        [&counter]()
        {
          std::this_thread::sleep_for(10ms);
          counter.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted == true);

    executor.stop_and_wait();

    assert(counter.load(std::memory_order_relaxed) == 1);
    assert_stopped_state(executor);
    assert_zero_metrics(executor);
  }

  static void test_stop_and_wait_disables_accepting_before_stop()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    executor.start();

    assert_started_state(executor);

    executor.stop_and_wait();

    assert_stopped_state(executor);

    const bool accepted = executor.submit(
        make_complete_task(1u, counter));

    assert(accepted == false);
    assert(counter.load(std::memory_order_relaxed) == 0);
    assert(executor.rejected_tasks() == 1u);
  }

  static void test_runtime_accessor_reflects_executor_lifecycle()
  {
    RuntimeExecutor executor{1u};

    Runtime &runtime = executor.runtime();

    assert(&runtime == &executor.runtime());

    assert(runtime.started() == false);
    assert(runtime.running() == false);

    executor.start();

    assert(&runtime == &executor.runtime());
    assert(runtime.started() == true);
    assert(runtime.running() == true);

    executor.stop();

    assert(&runtime == &executor.runtime());
    assert(runtime.started() == false);
    assert(runtime.running() == false);
  }

  static void test_const_runtime_accessor_reflects_executor_lifecycle()
  {
    RuntimeExecutor executor{1u};

    const RuntimeExecutor &const_executor = executor;

    const Runtime &runtime = const_executor.runtime();

    assert(&runtime == &const_executor.runtime());

    assert(runtime.started() == false);
    assert(runtime.running() == false);

    executor.start();

    assert(&runtime == &const_executor.runtime());
    assert(runtime.started() == true);
    assert(runtime.running() == true);

    executor.stop();

    assert(&runtime == &const_executor.runtime());
    assert(runtime.started() == false);
    assert(runtime.running() == false);
  }

  static void test_lifecycle_preserves_runtime_configuration()
  {
    RuntimeConfig config{
        2u,
        BudgetConfig{7u}};

    RuntimeExecutor executor{config};

    assert(executor.runtime().worker_count() == 2u);
    assert(executor.runtime().config().workerCount == 2u);
    assert(executor.runtime().config().budget.quantum == 7u);

    executor.start();

    assert(executor.runtime().worker_count() == 2u);
    assert(executor.runtime().config().workerCount == 2u);
    assert(executor.runtime().config().budget.quantum == 7u);

    executor.stop();

    assert(executor.runtime().worker_count() == 2u);
    assert(executor.runtime().config().workerCount == 2u);
    assert(executor.runtime().config().budget.quantum == 7u);

    executor.start();

    assert(executor.runtime().worker_count() == 2u);
    assert(executor.runtime().config().workerCount == 2u);
    assert(executor.runtime().config().budget.quantum == 7u);

    executor.stop();
  }

  static void test_counters_survive_lifecycle_transitions()
  {
    RuntimeExecutor executor{1u};

    std::atomic<int> counter{0};

    assert(executor.submit(make_complete_task(1u, counter)) == false);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);

    executor.start();

    assert(executor.submit(make_complete_task(2u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 1u);

    executor.stop();

    assert(executor.submit(make_complete_task(3u, counter)) == false);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 2u);

    executor.start();

    assert(executor.submit(make_complete_task(4u, counter)) == true);

    assert(wait_until(
        [&counter]()
        {
          return counter.load(std::memory_order_relaxed) == 2;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 2u);

    executor.stop();
  }

  static void test_multiple_executors_lifecycle_is_independent()
  {
    RuntimeExecutor first{1u};
    RuntimeExecutor second{1u};

    assert_stopped_state(first);
    assert_stopped_state(second);

    first.start();

    assert_started_state(first);
    assert_stopped_state(second);

    second.start();

    assert_started_state(first);
    assert_started_state(second);

    first.stop();

    assert_stopped_state(first);
    assert_started_state(second);

    second.stop();

    assert_stopped_state(first);
    assert_stopped_state(second);
  }

  static void test_destructor_stops_started_executor()
  {
    std::atomic<int> counter{0};

    {
      RuntimeExecutor executor{1u};

      executor.start();

      assert_started_state(executor);

      const bool accepted = executor.submit(
          make_complete_task(1u, counter));

      assert(accepted == true);

      assert(wait_until(
          [&counter]()
          {
            return counter.load(std::memory_order_relaxed) == 1;
          }));

      executor.wait_idle();
    }

    assert(counter.load(std::memory_order_relaxed) == 1);
  }

  static void test_destructor_is_safe_for_stopped_executor()
  {
    {
      RuntimeExecutor executor{1u};

      executor.start();

      assert_started_state(executor);

      executor.stop();

      assert_stopped_state(executor);
    }

    {
      RuntimeExecutor executor{1u};

      assert_stopped_state(executor);
    }
  }

} // namespace

int main()
{
  test_initial_lifecycle_state();

  test_start_transitions_to_started_running_accepting();
  test_start_is_idempotent();

  test_stop_before_start_is_safe();
  test_stop_after_start_transitions_to_stopped();
  test_stop_is_idempotent_after_start();

  test_executor_can_restart_after_stop();

  test_accepting_tracks_lifecycle();

  test_submit_is_rejected_before_start_and_after_stop();
  test_submit_executes_after_start();
  test_submit_executes_after_restart();

  test_wait_idle_is_safe_before_start();
  test_wait_idle_waits_for_started_work();

  test_stop_and_wait_before_start_is_safe();
  test_stop_and_wait_stops_started_executor();
  test_stop_and_wait_waits_for_current_work();
  test_stop_and_wait_disables_accepting_before_stop();

  test_runtime_accessor_reflects_executor_lifecycle();
  test_const_runtime_accessor_reflects_executor_lifecycle();
  test_lifecycle_preserves_runtime_configuration();

  test_counters_survive_lifecycle_transitions();

  test_multiple_executors_lifecycle_is_independent();

  test_destructor_stops_started_executor();
  test_destructor_is_safe_for_stopped_executor();

  return 0;
}
