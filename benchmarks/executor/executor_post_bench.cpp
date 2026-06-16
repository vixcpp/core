/**
 *
 * @file executor_post_bench.cpp
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
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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

  constexpr std::uint64_t kPostTasks = 100'000;
  constexpr std::uint64_t kCpuTasks = 50'000;
  constexpr std::uint64_t kOptionTasks = 100'000;
  constexpr std::uint64_t kExceptionTasks = 25'000;
  constexpr std::uint64_t kRejectTasks = 500'000;
  constexpr std::uint64_t kTimeoutTasks = 250;

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

  static vix::bench::BenchmarkResult bench_post_void_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.post/void_tasks",
            kPostTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kPostTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed, &sink]()
                        {
                          sink.fetch_add(1u, std::memory_order_relaxed);
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
                        return completed.load(std::memory_order_relaxed) == kPostTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kPostTasks);
              assert(executor.submitted_tasks() == kPostTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.fast_http_submitted_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_post_cpu_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.post/cpu_tasks",
            kCpuTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      32u);

              executor.start();

              for (std::uint64_t i = 0; i < kCpuTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed, &sink]()
                        {
                          std::uint64_t local = 0;

                          for (std::uint32_t j = 0; j < 128u; ++j)
                          {
                            local += static_cast<std::uint64_t>(j ^ (j << 1u));
                          }

                          sink.fetch_add(local, std::memory_order_relaxed);
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
                        return completed.load(std::memory_order_relaxed) == kCpuTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kCpuTasks);
              assert(executor.submitted_tasks() == kCpuTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_post_with_options()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    TaskOptions options;

    options.priority = 10;
    options.timeout = 0ms;
    options.deadline = 1000ms;
    options.may_block = true;

    auto result =
        vix::bench::run(
            "executor.post/with_options",
            kOptionTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kOptionTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed, &sink]()
                        {
                          sink.fetch_add(1u, std::memory_order_relaxed);
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
                        return completed.load(std::memory_order_relaxed) == kOptionTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kOptionTasks);
              assert(executor.submitted_tasks() == kOptionTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);
              assert(executor.metrics().timed_out == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_post_exceptions()
  {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.post/exceptions",
            kExceptionTasks,
            [&]()
            {
              calls.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kExceptionTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&calls, &sink]()
                        {
                          sink.fetch_add(1u, std::memory_order_relaxed);
                          calls.fetch_add(1u, std::memory_order_relaxed);

                          throw std::runtime_error("executor post benchmark failure");
                        });

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&calls]()
                      {
                        return calls.load(std::memory_order_relaxed) == kExceptionTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(calls.load(std::memory_order_relaxed) == kExceptionTasks);
              assert(executor.submitted_tasks() == kExceptionTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == kExceptionTasks);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_post_timeout_recording()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    TaskOptions options;

    options.timeout = 1ms;

    auto result =
        vix::bench::run(
            "executor.post/timeout_recording",
            kTimeoutTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kTimeoutTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&completed, &sink]()
                        {
                          std::this_thread::sleep_for(5ms);

                          sink.fetch_add(1u, std::memory_order_relaxed);
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
                        return completed.load(std::memory_order_relaxed) == kTimeoutTasks;
                      },
                      20'000ms);

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kTimeoutTasks);
              assert(executor.submitted_tasks() == kTimeoutTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);
              assert(executor.metrics().timed_out == kTimeoutTasks);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 1,
                .measure_iterations = 5,
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_reject_post_before_start()
  {
    std::atomic<std::uint64_t> calls{0};
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.post/reject_before_start",
            kRejectTasks,
            [&]()
            {
              calls.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              for (std::uint64_t i = 0; i < kRejectTasks; ++i)
              {
                std::function<void()> fn =
                    [&calls]()
                {
                  calls.fetch_add(1u, std::memory_order_relaxed);
                };

                const bool accepted =
                    executor.post(std::move(fn));

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(calls.load(std::memory_order_relaxed) == 0u);
              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kRejectTasks);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_reject_empty_callable_after_start()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.post/reject_empty_callable_after_start",
            kRejectTasks,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kRejectTasks; ++i)
              {
                const bool accepted =
                    executor.post(std::function<void()>{});

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kRejectTasks);
              assert(executor.failed_tasks() == 0u);

              executor.wait_idle();

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_reject_post_after_stop()
  {
    std::atomic<std::uint64_t> calls{0};
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.post/reject_after_stop",
            kRejectTasks,
            [&]()
            {
              calls.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              executor.start();
              executor.stop();

              for (std::uint64_t i = 0; i < kRejectTasks; ++i)
              {
                const bool accepted =
                    executor.post(
                        [&calls]()
                        {
                          calls.fetch_add(1u, std::memory_order_relaxed);
                        });

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(calls.load(std::memory_order_relaxed) == 0u);
              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kRejectTasks);
              assert(executor.failed_tasks() == 0u);

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

  results.push_back(bench_post_void_tasks());
  results.push_back(bench_post_cpu_tasks());
  results.push_back(bench_post_with_options());
  results.push_back(bench_post_exceptions());
  results.push_back(bench_post_timeout_recording());

  results.push_back(bench_reject_post_before_start());
  results.push_back(bench_reject_empty_callable_after_start());
  results.push_back(bench_reject_post_after_stop());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.executor.post",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
