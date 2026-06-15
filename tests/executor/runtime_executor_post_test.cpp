/**
 *
 * @file runtime_executor_post_test.cpp
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

  static void assert_empty_metrics(const RuntimeExecutor &executor)
  {
    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_post_before_start_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    std::function<void()> task =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
    };

    const bool accepted = executor.post(std::move(task));

    assert(!accepted);
    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);
  }

  static void test_post_null_callable_is_rejected_before_start()
  {
    RuntimeExecutor executor = make_executor();

    const bool accepted = executor.post(std::function<void()>{});

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);
  }

  static void test_post_null_callable_is_rejected_after_start()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    const bool accepted = executor.post(std::function<void()>{});

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    executor.wait_idle();

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_post_after_start_executes_void_task()
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

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_post_many_tasks_execute()
  {
    RuntimeExecutor executor = make_executor(4u, 8u);

    std::atomic<int> calls{0};

    executor.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = executor.post(
          [&calls]()
          {
            calls.fetch_add(1, std::memory_order_relaxed);
          });

      assert(accepted);
    }

    assert(executor.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    executor.wait_idle();

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_post_with_options_executes()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
    options.priority = 10;
    options.timeout = 100ms;
    options.deadline = 1000ms;
    options.may_block = true;

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

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
    assert(executor.metrics().timed_out == 0u);

    stop_executor(executor);
  }

  static void test_post_tracks_active_while_task_is_running()
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

    assert(executor.metrics().active >= 1u);

    release.store(true, std::memory_order_release);

    executor.wait_idle();

    assert(calls.load(std::memory_order_relaxed) == 1);
    assert(executor.metrics().active == 0u);

    stop_executor(executor);
  }

  static void test_post_exception_is_caught_and_failed_count_increments()
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

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 1u);

    assert(executor.metrics().active == 0u);
    assert(executor.metrics().timed_out == 0u);

    stop_executor(executor);
  }

  static void test_post_multiple_exceptions_increment_failed_count()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> calls{0};

    executor.start();

    constexpr int count = 10;

    for (int i = 0; i < count; ++i)
    {
      const bool accepted = executor.post(
          [&calls]()
          {
            calls.fetch_add(1, std::memory_order_relaxed);
            throw std::runtime_error("post failed");
          });

      assert(accepted);
    }

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == count;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == static_cast<std::uint64_t>(count));

    assert(executor.metrics().active == 0u);

    stop_executor(executor);
  }

  static void test_post_timeout_is_recorded_on_success()
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

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
    assert(executor.metrics().timed_out == 1u);
    assert(executor.metrics().active == 0u);

    stop_executor(executor);
  }

  static void test_post_timeout_is_recorded_on_exception()
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

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 1u);
    assert(executor.metrics().timed_out == 1u);
    assert(executor.metrics().active == 0u);

    stop_executor(executor);
  }

  static void test_post_no_timeout_when_timeout_is_zero()
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

    assert(executor.metrics().timed_out == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_no_timeout_when_elapsed_is_under_timeout()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskOptions options;
    options.timeout = 100ms;

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

    assert(executor.metrics().timed_out == 0u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_after_stop_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();
    stop_executor(executor);

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(!accepted);
    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_post_after_restart_executes()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();
    stop_executor(executor);

    assert(executor.post(
               [&calls]()
               {
                 calls.fetch_add(1, std::memory_order_relaxed);
               }) == false);

    assert(executor.rejected_tasks() == 1u);

    executor.start();

    const bool accepted = executor.post(
        [&calls]()
        {
          calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_post_http_fast_taskfn_before_start_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    TaskFn fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.post_http_fast(std::move(fn));

    assert(!accepted);
    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_post_http_fast_void_before_start_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    std::function<void()> fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
    };

    const bool accepted = executor.post_http_fast(std::move(fn));

    assert(!accepted);
    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

  static void test_post_http_fast_null_taskfn_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    const bool accepted = executor.post_http_fast(TaskFn{});

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_http_fast_null_void_callable_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    executor.start();

    const bool accepted = executor.post_http_fast(std::function<void()>{});

    assert(!accepted);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_http_fast_taskfn_executes()
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

    assert(executor.submitted_tasks() == 1u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_post_http_fast_void_callable_executes()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    std::function<void()> fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
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
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_post_http_fast_taskfn_with_affinity_executes()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> calls{0};

    executor.start();

    TaskFn fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.post_http_fast(std::move(fn), 1u);

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_post_http_fast_void_with_affinity_executes()
  {
    RuntimeExecutor executor = make_executor(2u, 8u);

    std::atomic<int> calls{0};

    executor.start();

    std::function<void()> fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
    };

    const bool accepted = executor.post_http_fast(std::move(fn), 1u);

    assert(accepted);

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == 1;
        }));

    assert(executor.submitted_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 1u);

    stop_executor(executor);
  }

  static void test_post_http_fast_many_tasks_execute()
  {
    RuntimeExecutor executor = make_executor(4u, 8u);

    std::atomic<int> calls{0};

    executor.start();

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
      TaskFn fn =
          [&calls]()
      {
        calls.fetch_add(1, std::memory_order_relaxed);
        return TaskResult::complete;
      };

      const bool accepted = executor.post_http_fast(std::move(fn));

      assert(accepted);
    }

    assert(wait_until(
        [&calls]()
        {
          return calls.load(std::memory_order_relaxed) == count;
        },
        2000ms));

    executor.wait_idle();

    assert(executor.submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(executor.fast_http_submitted_tasks() == static_cast<std::uint64_t>(count));
    assert(executor.rejected_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    assert_empty_metrics(executor);

    stop_executor(executor);
  }

  static void test_post_http_fast_failed_taskfn_is_not_post_failed()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();

    TaskFn fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::failed;
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

    /*
     * fast HTTP tasks do not use post() bookkeeping, so failed_tasks()
     * remains reserved for failures observed by post().
     */
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_http_fast_throwing_taskfn_is_not_post_failed()
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
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_http_fast_after_stop_is_rejected()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> calls{0};

    executor.start();
    stop_executor(executor);

    TaskFn fn =
        [&calls]()
    {
      calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    const bool accepted = executor.post_http_fast(std::move(fn));

    assert(!accepted);
    assert(calls.load(std::memory_order_relaxed) == 0);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 1u);
    assert(executor.fast_http_submitted_tasks() == 0u);
  }

  static void test_post_and_fast_path_counters_are_separate()
  {
    RuntimeExecutor executor = make_executor();

    std::atomic<int> normal_calls{0};
    std::atomic<int> fast_calls{0};

    executor.start();

    assert(executor.post(
        [&normal_calls]()
        {
          normal_calls.fetch_add(1, std::memory_order_relaxed);
        }));

    TaskFn fast =
        [&fast_calls]()
    {
      fast_calls.fetch_add(1, std::memory_order_relaxed);
      return TaskResult::complete;
    };

    assert(executor.post_http_fast(std::move(fast)));

    assert(wait_until(
        [&normal_calls, &fast_calls]()
        {
          return normal_calls.load(std::memory_order_relaxed) == 1 &&
                 fast_calls.load(std::memory_order_relaxed) == 1;
        }));

    executor.wait_idle();

    assert(executor.submitted_tasks() == 2u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 1u);
    assert(executor.failed_tasks() == 0u);

    stop_executor(executor);
  }

  static void test_post_and_fast_path_rejections_accumulate()
  {
    RuntimeExecutor executor = make_executor();

    assert(executor.post(std::function<void()>{}) == false);
    assert(executor.post_http_fast(TaskFn{}) == false);
    assert(executor.post_http_fast(std::function<void()>{}) == false);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 3u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);
  }

} // namespace

int main()
{
  test_post_before_start_is_rejected();
  test_post_null_callable_is_rejected_before_start();
  test_post_null_callable_is_rejected_after_start();

  test_post_after_start_executes_void_task();
  test_post_many_tasks_execute();

  test_post_with_options_executes();

  test_post_tracks_active_while_task_is_running();

  test_post_exception_is_caught_and_failed_count_increments();
  test_post_multiple_exceptions_increment_failed_count();

  test_post_timeout_is_recorded_on_success();
  test_post_timeout_is_recorded_on_exception();
  test_post_no_timeout_when_timeout_is_zero();
  test_post_no_timeout_when_elapsed_is_under_timeout();

  test_post_after_stop_is_rejected();
  test_post_after_restart_executes();

  test_post_http_fast_taskfn_before_start_is_rejected();
  test_post_http_fast_void_before_start_is_rejected();

  test_post_http_fast_null_taskfn_is_rejected();
  test_post_http_fast_null_void_callable_is_rejected();

  test_post_http_fast_taskfn_executes();
  test_post_http_fast_void_callable_executes();

  test_post_http_fast_taskfn_with_affinity_executes();
  test_post_http_fast_void_with_affinity_executes();

  test_post_http_fast_many_tasks_execute();

  test_post_http_fast_failed_taskfn_is_not_post_failed();
  test_post_http_fast_throwing_taskfn_is_not_post_failed();

  test_post_http_fast_after_stop_is_rejected();

  test_post_and_fast_path_counters_are_separate();
  test_post_and_fast_path_rejections_accumulate();

  return 0;
}
