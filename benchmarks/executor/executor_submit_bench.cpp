/**
 *
 * @file executor_submit_bench.cpp
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
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;

  using namespace std::chrono_literals;

  constexpr std::uint64_t kSubmitTasks = 100'000;
  constexpr std::uint64_t kCpuTasks = 50'000;
  constexpr std::uint64_t kYieldingTasks = 30'000;
  constexpr std::uint64_t kInvalidTasks = 500'000;
  constexpr std::uint32_t kYieldSteps = 5;

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

  static Task make_complete_task(
      TaskId id,
      std::atomic<std::uint64_t> &completed,
      std::atomic<std::uint64_t> &sink,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&completed, &sink]()
        {
          sink.fetch_add(1u, std::memory_order_relaxed);
          completed.fetch_add(1u, std::memory_order_relaxed);

          return TaskResult::complete;
        },
        affinity};
  }

  static Task make_cpu_task(
      TaskId id,
      std::atomic<std::uint64_t> &completed,
      std::atomic<std::uint64_t> &sink,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&completed, &sink]()
        {
          std::uint64_t local = 0;

          for (std::uint32_t i = 0; i < 128u; ++i)
          {
            local += static_cast<std::uint64_t>(i ^ (i << 1u));
          }

          sink.fetch_add(local, std::memory_order_relaxed);
          completed.fetch_add(1u, std::memory_order_relaxed);

          return TaskResult::complete;
        },
        affinity};
  }

  static Task make_yielding_task(
      TaskId id,
      std::atomic<std::uint64_t> &completed,
      std::atomic<std::uint64_t> &sink,
      std::uint32_t complete_after,
      std::uint32_t affinity = 0)
  {
    auto state =
        std::make_shared<std::uint32_t>(0u);

    return Task{
        id,
        [state, complete_after, &completed, &sink]() mutable
        {
          sink.fetch_add(1u, std::memory_order_relaxed);

          ++(*state);

          if (*state < complete_after)
          {
            return TaskResult::yield;
          }

          completed.fetch_add(1u, std::memory_order_relaxed);

          return TaskResult::complete;
        },
        affinity};
  }

  static Task make_failed_task(
      TaskId id,
      std::atomic<std::uint64_t> &calls,
      std::atomic<std::uint64_t> &sink,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&calls, &sink]()
        {
          sink.fetch_add(1u, std::memory_order_relaxed);
          calls.fetch_add(1u, std::memory_order_relaxed);

          return TaskResult::failed;
        },
        affinity};
  }

  static Task make_invalid_task(TaskId id)
  {
    return Task{
        id,
        TaskFn{}};
  }

  static vix::bench::BenchmarkResult bench_submit_task_complete()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.submit/task_complete",
            kSubmitTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kSubmitTasks; ++i)
              {
                const bool accepted =
                    executor.submit(
                        make_complete_task(
                            static_cast<TaskId>(i + 1u),
                            completed,
                            sink));

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kSubmitTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kSubmitTasks);
              assert(executor.submitted_tasks() == kSubmitTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_taskfn_complete()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.submit/taskfn_complete",
            kSubmitTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kSubmitTasks; ++i)
              {
                TaskFn fn =
                    [&completed, &sink]()
                {
                  sink.fetch_add(1u, std::memory_order_relaxed);
                  completed.fetch_add(1u, std::memory_order_relaxed);

                  return TaskResult::complete;
                };

                const bool accepted =
                    executor.submit(std::move(fn));

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kSubmitTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kSubmitTasks);
              assert(executor.submitted_tasks() == kSubmitTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_taskfn_with_affinity()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    const std::uint32_t workers =
        worker_count();

    auto result =
        vix::bench::run(
            "executor.submit/taskfn_affinity",
            kSubmitTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      workers,
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kSubmitTasks; ++i)
              {
                TaskFn fn =
                    [&completed, &sink]()
                {
                  sink.fetch_add(1u, std::memory_order_relaxed);
                  completed.fetch_add(1u, std::memory_order_relaxed);

                  return TaskResult::complete;
                };

                const std::uint32_t affinity =
                    static_cast<std::uint32_t>(i % workers);

                const bool accepted =
                    executor.submit(
                        std::move(fn),
                        affinity);

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kSubmitTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kSubmitTasks);
              assert(executor.submitted_tasks() == kSubmitTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_task_cpu()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.submit/task_cpu",
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
                    executor.submit(
                        make_cpu_task(
                            static_cast<TaskId>(i + 1u),
                            completed,
                            sink));

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

  static vix::bench::BenchmarkResult bench_submit_yielding_task()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.submit/task_yielding",
            kYieldingTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kYieldingTasks; ++i)
              {
                const bool accepted =
                    executor.submit(
                        make_yielding_task(
                            static_cast<TaskId>(i + 1u),
                            completed,
                            sink,
                            kYieldSteps));

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kYieldingTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kYieldingTasks);
              assert(executor.submitted_tasks() == kYieldingTasks);
              assert(executor.rejected_tasks() == 0u);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) >= kYieldingTasks);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_failed_task()
  {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "executor.submit/task_failed",
            kSubmitTasks,
            [&]()
            {
              calls.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              RuntimeExecutor executor =
                  make_executor(
                      worker_count(),
                      16u);

              executor.start();

              for (std::uint64_t i = 0; i < kSubmitTasks; ++i)
              {
                const bool accepted =
                    executor.submit(
                        make_failed_task(
                            static_cast<TaskId>(i + 1u),
                            calls,
                            sink));

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&calls]()
                      {
                        return calls.load(std::memory_order_relaxed) == kSubmitTasks;
                      });

              executor.wait_idle();

              assert(done);
              assert(calls.load(std::memory_order_relaxed) == kSubmitTasks);
              assert(executor.submitted_tasks() == kSubmitTasks);
              assert(executor.rejected_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_reject_invalid_task_after_start()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.submit/reject_invalid_task",
            kInvalidTasks,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kInvalidTasks; ++i)
              {
                const bool accepted =
                    executor.submit(
                        make_invalid_task(
                            static_cast<TaskId>(i + 1u)));

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(sink >= kInvalidTasks);
              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kInvalidTasks);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_reject_submit_before_start()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.submit/reject_before_start",
            kInvalidTasks,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              for (std::uint64_t i = 0; i < kInvalidTasks; ++i)
              {
                TaskFn fn =
                    [&sink]()
                {
                  ++sink;
                  return TaskResult::complete;
                };

                const bool accepted =
                    executor.submit(std::move(fn));

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kInvalidTasks);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_reject_terminal_state_task()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "executor.submit/reject_terminal_state_task",
            kInvalidTasks,
            [&]()
            {
              RuntimeExecutor executor =
                  make_executor(
                      1u,
                      8u);

              executor.start();

              for (std::uint64_t i = 0; i < kInvalidTasks; ++i)
              {
                Task task =
                    make_invalid_task(
                        static_cast<TaskId>(i + 1u));

                task.fn =
                    [&sink]()
                {
                  ++sink;
                  return TaskResult::complete;
                };

                task.state = TaskState::completed;

                const bool accepted =
                    executor.submit(std::move(task));

                sink += static_cast<std::uint64_t>(!accepted);
              }

              assert(executor.submitted_tasks() == 0u);
              assert(executor.rejected_tasks() == kInvalidTasks);
              assert(executor.failed_tasks() == 0u);

              assert_empty_live_metrics(executor);

              stop_executor(executor);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_submit_task_complete());
  results.push_back(bench_submit_taskfn_complete());
  results.push_back(bench_submit_taskfn_with_affinity());
  results.push_back(bench_submit_task_cpu());
  results.push_back(bench_submit_yielding_task());
  results.push_back(bench_submit_failed_task());

  results.push_back(bench_reject_invalid_task_after_start());
  results.push_back(bench_reject_submit_before_start());
  results.push_back(bench_reject_terminal_state_task());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.executor.submit",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
