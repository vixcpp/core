/**
 *
 * @file runtime_executor_metrics_test.cpp
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
#include <stdexcept>
#include <thread>
#include <utility>

#include <vix/executor/Metrics.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/executor/TaskOptions.hpp>
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
  using TaskOptions = vix::executor::TaskOptions;
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

  static void stop_executor(RuntimeExecutor &executor)
  {
    executor.stop();

    assert(executor.started() == false);
    assert(executor.running() == false);
    assert(executor.accepting() == false);
  }

  static void assert_metrics_are_zero(const RuntimeExecutor &executor)
  {
    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_initial_metrics_are_zero()
  {
    RuntimeExecutor executor = make_executor();

    assert_metrics_are_zero(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_metrics_are_zero_after_start_without_work()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    assert(executor.started());
    assert(executor.running());
    assert(executor.accepting());

    assert_metrics_are_zero(executor);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_active_is_visible_while_post_task_runs()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
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

    Metrics during = executor.metrics();

    assert(during.active >= 1u);
    assert(during.timed_out == 0u);

    release.store(true, std::memory_order_release);

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 1);

    Metrics after = executor.metrics();

    assert(after.pending == 0u);
    assert(after.active == 0u);
    assert(after.timed_out == 0u);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_pending_is_visible_when_worker_is_busy()
  {
    RuntimeExecutor executor = make_executor(1u, 8u);

    std::atomic<bool> first_entered{false};
    std::atomic<bool> release_first{false};
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

    Metrics during = executor.metrics();

    assert(during.pending >= 1u);
    assert(during.active >= 1u);
    assert(during.timed_out == 0u);

    release_first.store(true, std::memory_order_release);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 2;
        },
        2000ms));

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 2);

    assert_metrics_are_zero(executor);

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_pending_tracks_raw_runtime_submit_queue()
  {
    RuntimeExecutor executor = make_executor(1u, 8u);

    std::atomic<bool> first_entered{false};
    std::atomic<bool> release_first{false};
    std::atomic<int> calls{0};

    executor.start();

    const bool first_accepted = executor.submit(
        Task{
            1u,
            [&first_entered, &release_first, &calls]()
            {
              first_entered.store(true, std::memory_order_release);

              while (!release_first.load(std::memory_order_acquire))
              {
                std::this_thread::sleep_for(1ms);
              }

              calls.fetch_add(1, std::memory_order_relaxed);

              return TaskResult::complete;
            }});

    assert(first_accepted);

    assert(wait_until(
        [&first_entered]()
        {
          return first_entered.load(std::memory_order_acquire);
        }));

    const bool second_accepted = executor.submit(
        Task{
            2u,
            [&calls]()
            {
              calls.fetch_add(1, std::memory_order_relaxed);
              return TaskResult::complete;
            }});

    assert(second_accepted);

    assert(wait_until(
        [&executor]()
        {
          return executor.metrics().pending >= 1u;
        }));

    Metrics during = executor.metrics();

    assert(during.pending >= 1u);

    /*
     * Raw submit() does not use post() active bookkeeping.
     */
    assert(during.active == 0u);
    assert(during.timed_out == 0u);

    release_first.store(true, std::memory_order_release);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 2;
        },
        2000ms));

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 2);
    assert_metrics_are_zero(executor);

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_active_returns_to_zero_after_post_exception()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
          throw std::runtime_error("post failed");
        });

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_metrics_timeout_is_recorded_for_slow_successful_post()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
    options.timeout = 1ms;

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          std::this_thread::sleep_for(10ms);
          calls.fetch_add(1, std::memory_order_relaxed);
        },
        options);

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 1u);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_timeout_is_recorded_for_slow_throwing_post()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
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

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 1u);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_metrics_timeout_accumulates()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
    options.timeout = 1ms;

    executor.start();

    for (int i = 0; i < 3; ++i)
    {
      const bool accepted = executor.post(
          [&calls]()
          {
            std::this_thread::sleep_for(10ms);
            calls.fetch_add(1, std::memory_order_relaxed);
          },
          options);

      assert(accepted);
    }

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 3;
        },
        2000ms));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 3u);

    assert(executor.submitted_tasks() == 3u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_timeout_is_not_recorded_when_timeout_is_zero()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
    options.timeout = 0ms;

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          std::this_thread::sleep_for(5ms);
          calls.fetch_add(1, std::memory_order_relaxed);
        },
        options);

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_metrics_timeout_is_not_recorded_when_under_timeout()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
    options.timeout = 200ms;

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        },
        options);

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_fast_http_path_updates_fast_counter_but_not_active_metrics()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    TaskFn fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.post_http_fast(std::move(fn));

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_fast_http_void_path_updates_fast_counter()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    const bool accepted = executor.post_http_fast(
        std::function<void()>{
            [&calls]()
            {
              calls.fetch_add(1, std::memory_order_relaxed);
            }});

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert_metrics_are_zero(executor);

    stop_executor(executor);
  }

  static void test_fast_http_throwing_task_does_not_increment_post_failed_counter()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    TaskFn fn =
        [&calls]() -> TaskResult
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      throw std::runtime_error("fast task failed");
    };

    const bool accepted = executor.post_http_fast(std::move(fn));

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    /*
     * failed_tasks() is reserved for post() wrapper failures.
     */
    assert(executor.failed_tasks() == 0u);

    assert_metrics_are_zero(executor);

    stop_executor(executor);
  }

  static void test_rejected_tasks_do_not_change_metrics()
  {
    RuntimeExecutor executor = make_executor();

    const bool rejected_post = executor.post(std::function<void()>{});
    const bool rejected_fast = executor.post_http_fast(TaskFn{});
    const bool rejected_submit = executor.submit(TaskFn{});

    assert(!rejected_post);
    assert(!rejected_fast);
    assert(!rejected_submit);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 3u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert_metrics_are_zero(executor);
  }

  static void test_metrics_after_stop_remain_readable()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    assert(executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        }));

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    stop_executor(executor);

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
  }

  static void test_metrics_survive_restart()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    assert(executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        }));

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();
    executor.stop();

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);

    executor.start();

    assert(executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        }));

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 2;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 0u);

    assert_metrics_are_zero(executor);

    stop_executor(executor);
  }

  static void test_failed_and_timeout_metrics_can_both_increment()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
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

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.failed_tasks() == 1u);

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 1u);

    stop_executor(executor);
  }

  static void test_active_metric_is_zero_after_stop_and_wait()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          std::this_thread::sleep_for(10ms);
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    executor.stop_and_wait();

    assert(calls.load(std::memory_order_relaxed) == 1);

    Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.started() == false);
    assert(executor.running() == false);
    assert(executor.accepting() == false);
  }

} // namespace

int main()
{
  test_initial_metrics_are_zero();
  test_metrics_are_zero_after_start_without_work();

  test_metrics_active_is_visible_while_post_task_runs();
  test_metrics_pending_is_visible_when_worker_is_busy();
  test_metrics_pending_tracks_raw_runtime_submit_queue();

  test_metrics_active_returns_to_zero_after_post_exception();

  test_metrics_timeout_is_recorded_for_slow_successful_post();
  test_metrics_timeout_is_recorded_for_slow_throwing_post();
  test_metrics_timeout_accumulates();

  test_metrics_timeout_is_not_recorded_when_timeout_is_zero();
  test_metrics_timeout_is_not_recorded_when_under_timeout();

  test_fast_http_path_updates_fast_counter_but_not_active_metrics();
  test_fast_http_void_path_updates_fast_counter();
  test_fast_http_throwing_task_does_not_increment_post_failed_counter();

  test_rejected_tasks_do_not_change_metrics();

  test_metrics_after_stop_remain_readable();
  test_metrics_survive_restart();

  test_failed_and_timeout_metrics_can_both_increment();
  test_active_metric_is_zero_after_stop_and_wait();

  return 0;
}
