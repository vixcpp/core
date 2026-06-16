/**
 *
 * @file executor_metrics_bench.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include "common/Benchmark.hpp"
#include "common/BenchmarkJson.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <vix/executor/Metrics.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/executor/TaskOptions.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Metrics = vix::executor::Metrics;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;
  using TaskOptions = vix::executor::TaskOptions;

  using namespace std::chrono_literals;

  constexpr std::uint64_t kMetricReads = 1'000'000;
  constexpr std::uint64_t kActiveMetricReads = 500'000;
  constexpr std::uint64_t kPendingMetricReads = 500'000;
  constexpr std::uint64_t kTimedOutTasks = 250;
  constexpr std::uint64_t kCompletedTasks = 100'000;
  constexpr std::uint64_t kPendingTasks = 25'000;

  template <class Predicate>
  static bool wait_until(
      Predicate predicate,
      std::chrono::milliseconds timeout = 10'000ms)
  {
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

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

  static std::uint32_t worker_count()
  {
    const unsigned int hardware =
        std::thread::hardware_concurrency();

    if (hardware == 0u)
    {
      return 1u;
    }

    return static_cast<std::uint32_t>(hardware);
  }

  static RuntimeExecutor make_executor(
      std::uint32_t workers,
      std::uint32_t quantum)
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

  static void assert_empty_live_metrics(const RuntimeExecutor &executor)
  {
    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
  }

  static vix::bench::BenchmarkResult bench_metrics_idle_reads()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/idle_reads",
            kMetricReads,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              for (std::uint64_t i = 0; i < kMetricReads; ++i)
              {
                const Metrics metrics = executor.metrics();

                sink += metrics.pending;
                sink += metrics.active;
                sink += metrics.timed_out;
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              vix::bench::do_not_optimize(sink);
            });

    return result;
  }

  static vix::bench::BenchmarkResult bench_metrics_running_idle_reads()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/running_idle_reads",
            kMetricReads,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kMetricReads; ++i)
              {
                const Metrics metrics = executor.metrics();

                sink += metrics.pending;
                sink += metrics.active;
                sink += metrics.timed_out;
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            });

    return result;
  }

  static vix::bench::BenchmarkResult bench_metrics_active_reads()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/active_reads",
            kActiveMetricReads,
            [&]()
            {
              std::atomic<bool> release{false};
              std::atomic<bool> entered{false};

              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              executor.start();

              const bool accepted =
                  executor.post(
                      [&release, &entered]()
                      {
                        entered.store(true, std::memory_order_release);

                        while (!release.load(std::memory_order_acquire))
                        {
                          std::this_thread::yield();
                        }
                      });

              assert(accepted);

              const bool active =
                  wait_until(
                      [&executor, &entered]()
                      {
                        return entered.load(std::memory_order_acquire) &&
                               executor.metrics().active == 1u;
                      });

              assert(active);

              for (std::uint64_t i = 0; i < kActiveMetricReads; ++i)
              {
                const Metrics metrics = executor.metrics();

                sink += metrics.pending;
                sink += metrics.active;
                sink += metrics.timed_out;
              }

              release.store(true, std::memory_order_release);

              executor.wait_idle();

              assert_empty_live_metrics(executor);
              assert(executor.submitted_tasks() == 1u);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_metrics_pending_reads()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/pending_reads",
            kPendingMetricReads,
            [&]()
            {
              std::atomic<bool> release{false};
              std::atomic<bool> entered{false};
              std::atomic<std::uint64_t> completed{0};

              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              executor.start();

              const bool accepted_blocker =
                  executor.post(
                      [&release, &entered]()
                      {
                        entered.store(true, std::memory_order_release);

                        while (!release.load(std::memory_order_acquire))
                        {
                          std::this_thread::yield();
                        }
                      });

              assert(accepted_blocker);

              const bool blocker_active =
                  wait_until(
                      [&executor, &entered]()
                      {
                        return entered.load(std::memory_order_acquire) &&
                               executor.metrics().active == 1u;
                      });

              assert(blocker_active);

              for (std::uint64_t i = 0; i < kPendingTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed]()
                        {
                          completed.fetch_add(1u, std::memory_order_relaxed);
                        });

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool has_pending =
                  wait_until(
                      [&executor]()
                      {
                        return executor.metrics().pending > 0u;
                      });

              assert(has_pending);

              for (std::uint64_t i = 0; i < kPendingMetricReads; ++i)
              {
                const Metrics metrics = executor.metrics();

                sink += metrics.pending;
                sink += metrics.active;
                sink += metrics.timed_out;
              }

              release.store(true, std::memory_order_release);

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kPendingTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kPendingTasks);
              assert(executor.submitted_tasks() == kPendingTasks + 1u);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 1,
                .measure_iterations = 7,
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_metrics_after_completed_work()
  {
    std::atomic<std::uint64_t> completed{0};
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/after_completed_work",
            kCompletedTasks + kMetricReads,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kCompletedTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed]()
                        {
                          completed.fetch_add(1u, std::memory_order_relaxed);
                        });

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kCompletedTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kCompletedTasks);
              assert(executor.submitted_tasks() == kCompletedTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              for (std::uint64_t i = 0; i < kMetricReads; ++i)
              {
                const Metrics metrics = executor.metrics();

                sink += metrics.pending;
                sink += metrics.active;
                sink += metrics.timed_out;
              }

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            });

    return result;
  }

  static vix::bench::BenchmarkResult bench_metrics_timeout_counter_reads()
  {
    std::atomic<std::uint64_t> completed{0};
    std::uint64_t sink = 0;

    TaskOptions options;
    options.timeout = 1ms;

    auto result =
        vix::bench::run(
            "executor.metrics/timeout_counter_reads",
            kTimedOutTasks + kMetricReads,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kTimedOutTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed]()
                        {
                          std::this_thread::sleep_for(5ms);

                          completed.fetch_add(1u, std::memory_order_relaxed);
                        },
                        options);

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kTimedOutTasks;
                      },
                      20'000ms);

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kTimedOutTasks);
              assert(executor.submitted_tasks() == kTimedOutTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);
              assert(executor.metrics().timed_out == kTimedOutTasks);

              assert(executor.metrics().pending == 0u);
              assert(executor.metrics().active == 0u);

              for (std::uint64_t i = 0; i < kMetricReads; ++i)
              {
                const Metrics metrics = executor.metrics();

                sink += metrics.pending;
                sink += metrics.active;
                sink += metrics.timed_out;
              }

              assert(executor.metrics().timed_out == kTimedOutTasks);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 1,
                .measure_iterations = 5,
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_counter_accessors()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/counter_accessors",
            kMetricReads * 4u,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              for (std::uint64_t i = 0; i < kMetricReads; ++i)
              {
                sink += executor.submitted_tasks();
                sink += executor.rejected_tasks();
                sink += executor.fast_http_submitted_tasks();
                sink += executor.failed_tasks();
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.fast_http_submitted_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              vix::bench::do_not_optimize(sink);
            });

    return result;
  }

  static vix::bench::BenchmarkResult bench_counter_accessors_after_rejections()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.metrics/counter_accessors_after_rejections",
            kMetricReads,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              for (std::uint64_t i = 0; i < kPendingTasks; ++i)
              {
                const bool accepted =
                    executor.post(std::function<void()>{});

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kPendingTasks);
              assert(executor.fast_http_submitted_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              for (std::uint64_t i = 0; i < kMetricReads; ++i)
              {
                sink += executor.submitted_tasks();
                sink += executor.rejected_tasks();
                sink += executor.fast_http_submitted_tasks();
                sink += executor.failed_tasks();
              }

              assert_empty_live_metrics(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_metrics_idle_reads());
  results.push_back(bench_metrics_running_idle_reads());
  results.push_back(bench_metrics_active_reads());
  results.push_back(bench_metrics_pending_reads());
  results.push_back(bench_metrics_after_completed_work());
  results.push_back(bench_metrics_timeout_counter_reads());
  results.push_back(bench_counter_accessors());
  results.push_back(bench_counter_accessors_after_rejections());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.executor.metrics",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
