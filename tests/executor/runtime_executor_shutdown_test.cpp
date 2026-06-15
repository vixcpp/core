/**
 *
 * @file runtime_executor_shutdown_test.cpp
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
#include <cstdint>
#include <functional>
#include <thread>
#include <utility>

#include <vix/executor/Metrics.hpp>
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

  static RuntimeExecutor make_executor(
      std::uint32_t workers = 1u,
      std::uint32_t quantum = 8u)
  {
    return RuntimeExecutor{
        RuntimeConfig{
            workers,
            BudgetConfig{quantum}}};
  }

  static void assert_stopped(const RuntimeExecutor &executor)
  {
    assert(executor.started() == false);
    assert(executor.running() == false);
    assert(executor.accepting() == false);
    assert(executor.runtime().started() == false);
    assert(executor.runtime().running() == false);
  }

  static void assert_started(const RuntimeExecutor &executor)
  {
    assert(executor.started() == true);
    assert(executor.running() == true);
    assert(executor.accepting() == true);
    assert(executor.runtime().started() == true);
    assert(executor.runtime().running() == true);
  }

  static void assert_zero_live_metrics(const RuntimeExecutor &executor)
  {
    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
  }

  static void test_stop_before_start_is_safe_and_idempotent()
  {
    RuntimeExecutor executor = make_executor();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    executor.stop();
    executor.stop();
    executor.stop();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_stop_after_start_disables_accepting()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    assert_started(executor);

    executor.stop();

    assert_stopped(executor);

    std::atomic<int> calls{0};

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(!accepted);
    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
  }

  static void test_stop_after_start_is_idempotent()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    assert_started(executor);

    executor.stop();

    assert_stopped(executor);

    executor.stop();
    executor.stop();
    executor.stop();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);
  }

  static void test_stop_and_wait_before_start_is_safe_and_idempotent()
  {
    RuntimeExecutor executor = make_executor();

    assert_stopped(executor);

    executor.stop_and_wait();
    executor.stop_and_wait();
    executor.stop_and_wait();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_stop_and_wait_stops_idle_executor()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    assert_started(executor);

    executor.stop_and_wait();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);
  }

  static void test_stop_and_wait_waits_for_active_post_task()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<bool> shutdown_done{false};
    std::atomic<int> calls{0};

    executor.start();

    const bool accepted = executor.post(
        [&entered, &release, &calls]()
        {
          entered.store(true, std::memory_order_release);

          while (!release.load(std::memory_order_acquire))
          {
            std::this_thread::sleep_for(1ms);
          }

          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    assert(wait_until(
        [&entered]()
        {
          return entered.load(std::memory_order_acquire);
        }));

    assert(wait_until(
        [&executor]()
        {
          return executor.metrics().active >= 1u;
        }));

    std::thread shutdown_thread(
        [&executor, &shutdown_done]()
        {
          executor.stop_and_wait();
          shutdown_done.store(true, std::memory_order_release);
        });

    assert(wait_until(
        [&executor]()
        {
          return !executor.accepting();
        }));

    std::this_thread::sleep_for(5ms);

    assert(!shutdown_done.load(std::memory_order_acquire));
    assert(executor.metrics().active >= 1u);

    release.store(true, std::memory_order_release);

    shutdown_thread.join();

    assert(shutdown_done.load(std::memory_order_acquire));
    assert(calls.load(std::memory_order_relaxed) == 1);

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_stop_and_wait_waits_for_pending_post_task()
  {
    RuntimeExecutor executor = make_executor(1u, 8u);

    std::atomic<bool> first_entered{false};
    std::atomic<bool> release_first{false};
    std::atomic<bool> shutdown_done{false};
    std::atomic<int> calls{0};

    executor.start();

    const bool first_accepted = executor.post(
        [&first_entered, &release_first, &calls]()
        {
          first_entered.store(true, std::memory_order_release);

          while (!release_first.load(std::memory_order_acquire))
          {
            std::this_thread::sleep_for(1ms);
          }

          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(first_accepted);

    assert(wait_until(
        [&first_entered]()
        {
          return first_entered.load(std::memory_order_acquire);
        }));

    const bool second_accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(second_accepted);

    assert(wait_until(
        [&executor]()
        {
          return executor.metrics().pending >= 1u;
        }));

    std::thread shutdown_thread(
        [&executor, &shutdown_done]()
        {
          executor.stop_and_wait();
          shutdown_done.store(true, std::memory_order_release);
        });

    assert(wait_until(
        [&executor]()
        {
          return !executor.accepting();
        }));

    std::this_thread::sleep_for(5ms);

    assert(!shutdown_done.load(std::memory_order_acquire));
    assert(executor.metrics().active >= 1u);
    assert(executor.metrics().pending >= 1u);

    release_first.store(true, std::memory_order_release);

    shutdown_thread.join();

    assert(shutdown_done.load(std::memory_order_acquire));
    assert(calls.load(std::memory_order_relaxed) == 2);

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_stop_and_wait_rejects_new_work_during_shutdown()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<bool> shutdown_done{false};
    std::atomic<int> calls{0};
    std::atomic<int> rejected_calls{0};

    executor.start();

    const bool accepted = executor.post(
        [&entered, &release, &calls]()
        {
          entered.store(true, std::memory_order_release);

          while (!release.load(std::memory_order_acquire))
          {
            std::this_thread::sleep_for(1ms);
          }

          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    assert(wait_until(
        [&entered]()
        {
          return entered.load(std::memory_order_acquire);
        }));

    std::thread shutdown_thread(
        [&executor, &shutdown_done]()
        {
          executor.stop_and_wait();
          shutdown_done.store(true, std::memory_order_release);
        });

    assert(wait_until(
        [&executor]()
        {
          return !executor.accepting();
        }));

    const bool rejected = executor.post(
        [&rejected_calls]()
        {
          rejected_calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(!rejected);
    assert(rejected_calls.load(std::memory_order_relaxed) == 0);

    release.store(true, std::memory_order_release);

    shutdown_thread.join();

    assert(shutdown_done.load(std::memory_order_acquire));
    assert(calls.load(std::memory_order_relaxed) == 1);
    assert(rejected_calls.load(std::memory_order_relaxed) == 0);

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 1u);
  }

  static void test_wait_idle_waits_without_stopping_executor()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    assert_started(executor);

    const bool accepted = executor.post(
        [&calls]()
        {
          std::this_thread::sleep_for(10ms);
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 1);

    assert_started(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    executor.stop();

    assert_stopped(executor);
  }

  static void test_wait_idle_after_stop_is_safe()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();
    executor.stop();

    assert_stopped(executor);

    executor.wait_idle();
    executor.wait_idle();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);
  }

  static void test_stop_and_wait_after_wait_idle_is_safe()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 1);
    assert_started(executor);
    assert_zero_live_metrics(executor);

    executor.stop_and_wait();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);
  }

  static void test_stop_after_stop_and_wait_is_safe()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    assert_started(executor);

    executor.stop_and_wait();

    assert_stopped(executor);

    executor.stop();
    executor.stop();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);
  }

  static void test_stop_and_wait_after_stop_is_safe()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    assert_started(executor);

    executor.stop();

    assert_stopped(executor);

    executor.stop_and_wait();
    executor.stop_and_wait();

    assert_stopped(executor);
    assert_zero_live_metrics(executor);
  }

  static void test_submit_is_rejected_after_stop_and_wait()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    executor.stop_and_wait();

    assert_stopped(executor);

    const bool post_accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    const bool submit_accepted = executor.submit(
        Task{
            1u,
            [&calls]()
            {
              calls.fetch_add(1, std::memory_order_relaxed);
              return TaskResult::complete;
            }});

    TaskFn fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool fast_accepted = executor.post_http_fast(std::move(fn));

    assert(!post_accepted);
    assert(!submit_accepted);
    assert(!fast_accepted);

    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 3u);
    assert(executor.fast_http_submitted_tasks() == 0u);
  }

  static void test_executor_can_restart_after_stop_and_wait()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    executor.stop_and_wait();

    assert_stopped(executor);

    assert(executor.post(
               [&calls]()
               {
                 calls.fetch_add(1, std::memory_order_relaxed);
               }) == false);

    assert(executor.rejected_tasks() == 1u);

    executor.start();

    assert_started(executor);

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 1);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 1u);

    executor.stop();

    assert_stopped(executor);
  }

  static void test_stop_and_wait_preserves_timeout_and_failure_metrics()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    vix::executor::TaskOptions options;
    options.timeout = 1ms;

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          std::this_thread::sleep_for(10ms);
          calls.fetch_add(1, std::memory_order_relaxed);
          throw std::runtime_error("slow failure");
        },
        options);

    assert(accepted);

    executor.stop_and_wait();

    assert(calls.load(std::memory_order_relaxed) == 1);

    assert_stopped(executor);
    assert_zero_live_metrics(executor);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 1u);
    assert(executor.metrics().timed_out == 1u);
  }

  static void test_destructor_is_safe_for_never_started_executor()
  {
    {
      RuntimeExecutor executor = make_executor();

      assert_stopped(executor);
      assert_zero_live_metrics(executor);
    }
  }

  static void test_destructor_is_safe_for_started_idle_executor()
  {
    {
      RuntimeExecutor executor = make_executor();

      executor.start();

      assert_started(executor);
      assert_zero_live_metrics(executor);
    }
  }

  static void test_destructor_is_safe_after_explicit_stop()
  {
    {
      RuntimeExecutor executor = make_executor();

      executor.start();
      executor.stop();

      assert_stopped(executor);
    }
  }

  static void test_destructor_is_safe_after_stop_and_wait()
  {
    std::atomic<int> calls{0};

    {
      RuntimeExecutor executor = make_executor();

      executor.start();

      const bool accepted = executor.post(
          [&calls]()
          {
            calls.fetch_add(1, std::memory_order_relaxed);
          });

      assert(accepted);

      executor.stop_and_wait();

      assert(calls.load(std::memory_order_relaxed) == 1);
      assert_stopped(executor);
    }

    assert(calls.load(std::memory_order_relaxed) == 1);
  }

  static void test_multiple_executors_shutdown_independently()
  {
    RuntimeExecutor first = make_executor();
    RuntimeExecutor second = make_executor();

    std::atomic<int> first_calls{0};
    std::atomic<int> second_calls{0};

    first.start();
    second.start();

    assert(first.post(
        [&first_calls]()
        {
          first_calls.fetch_add(1, std::memory_order_relaxed);
        }));

    assert(second.post(
        [&second_calls]()
        {
          second_calls.fetch_add(1, std::memory_order_relaxed);
        }));

    first.stop_and_wait();

    assert(first_calls.load(std::memory_order_relaxed) == 1);
    assert_stopped(first);
    assert_started(second);

    second.wait_idle();

    assert(second_calls.load(std::memory_order_relaxed) == 1);
    assert_started(second);

    second.stop();

    assert_stopped(second);

    assert(first.submitted_tasks() == 1u);
    assert(second.submitted_tasks() == 1u);
  }

} // namespace

int main()
{
  test_stop_before_start_is_safe_and_idempotent();

  test_stop_after_start_disables_accepting();
  test_stop_after_start_is_idempotent();

  test_stop_and_wait_before_start_is_safe_and_idempotent();
  test_stop_and_wait_stops_idle_executor();

  test_stop_and_wait_waits_for_active_post_task();
  test_stop_and_wait_waits_for_pending_post_task();
  test_stop_and_wait_rejects_new_work_during_shutdown();

  test_wait_idle_waits_without_stopping_executor();
  test_wait_idle_after_stop_is_safe();

  test_stop_and_wait_after_wait_idle_is_safe();

  test_stop_after_stop_and_wait_is_safe();
  test_stop_and_wait_after_stop_is_safe();

  test_submit_is_rejected_after_stop_and_wait();
  test_executor_can_restart_after_stop_and_wait();

  test_stop_and_wait_preserves_timeout_and_failure_metrics();

  test_destructor_is_safe_for_never_started_executor();
  test_destructor_is_safe_for_started_idle_executor();
  test_destructor_is_safe_after_explicit_stop();
  test_destructor_is_safe_after_stop_and_wait();

  test_multiple_executors_shutdown_independently();

  return 0;
}
