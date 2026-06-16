/**
 *
 * @file runtime_scheduler_bench.cpp
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
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Scheduler.hpp>
#include <vix/runtime/Task.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Scheduler = vix::runtime::Scheduler;
  using Task = vix::runtime::Task;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;

  using namespace std::chrono_literals;

  constexpr std::uint64_t kSubmitTasks = 100'000;
  constexpr std::uint64_t kYieldingTasks = 30'000;
  constexpr std::uint64_t kAffinityTasks = 100'000;
  constexpr std::uint64_t kStartStopIterations = 50;
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
      std::uint32_t steps,
      std::uint32_t affinity = 0)
  {
    auto state =
        std::make_shared<std::uint32_t>(0u);

    return Task{
        id,
        [state, steps, &completed, &sink]() mutable
        {
          sink.fetch_add(1u, std::memory_order_relaxed);

          ++(*state);

          if (*state < steps)
          {
            return TaskResult::yield;
          }

          completed.fetch_add(1u, std::memory_order_relaxed);

          return TaskResult::complete;
        },
        affinity};
  }

  static vix::bench::BenchmarkResult bench_start_stop()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.scheduler/start_stop",
            kStartStopIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kStartStopIterations; ++i)
              {
                Scheduler scheduler{
                    1u,
                    BudgetConfig{8u}};

                scheduler.start();

                sink += static_cast<std::uint64_t>(scheduler.running());

                scheduler.stop();

                sink += static_cast<std::uint64_t>(!scheduler.running());
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 1,
                .measure_iterations = 7,
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_complete_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "runtime.scheduler/submit_complete_tasks",
            kSubmitTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Scheduler scheduler{
                  worker_count(),
                  BudgetConfig{16u}};

              scheduler.start();

              for (std::uint64_t i = 0; i < kSubmitTasks; ++i)
              {
                const bool accepted =
                    scheduler.submit(
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

              scheduler.stop();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kSubmitTasks);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_cpu_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "runtime.scheduler/submit_cpu_tasks",
            kSubmitTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Scheduler scheduler{
                  worker_count(),
                  BudgetConfig{32u}};

              scheduler.start();

              for (std::uint64_t i = 0; i < kSubmitTasks; ++i)
              {
                const bool accepted =
                    scheduler.submit(
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
                        return completed.load(std::memory_order_relaxed) == kSubmitTasks;
                      });

              scheduler.stop();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kSubmitTasks);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_yielding_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "runtime.scheduler/submit_yielding_tasks",
            kYieldingTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Scheduler scheduler{
                  worker_count(),
                  BudgetConfig{8u}};

              scheduler.start();

              for (std::uint64_t i = 0; i < kYieldingTasks; ++i)
              {
                const bool accepted =
                    scheduler.submit(
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

              scheduler.stop();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kYieldingTasks);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) >= kYieldingTasks);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_affinity_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    const std::uint32_t workers =
        worker_count();

    auto result =
        vix::bench::run(
            "runtime.scheduler/submit_affinity_tasks",
            kAffinityTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Scheduler scheduler{
                  workers,
                  BudgetConfig{16u}};

              scheduler.start();

              for (std::uint64_t i = 0; i < kAffinityTasks; ++i)
              {
                const std::uint32_t affinity =
                    static_cast<std::uint32_t>(i % workers);

                const bool accepted =
                    scheduler.submit(
                        make_complete_task(
                            static_cast<TaskId>(i + 1u),
                            completed,
                            sink,
                            affinity));

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kAffinityTasks;
                      });

              scheduler.stop();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kAffinityTasks);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_large_affinity_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "runtime.scheduler/submit_large_affinity_tasks",
            kAffinityTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Scheduler scheduler{
                  worker_count(),
                  BudgetConfig{16u}};

              scheduler.start();

              for (std::uint64_t i = 0; i < kAffinityTasks; ++i)
              {
                const bool accepted =
                    scheduler.submit(
                        make_complete_task(
                            static_cast<TaskId>(i + 1u),
                            completed,
                            sink,
                            999u));

                if (!accepted)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kAffinityTasks;
                      });

              scheduler.stop();

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kAffinityTasks);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_start_stop());
  results.push_back(bench_submit_complete_tasks());
  results.push_back(bench_submit_cpu_tasks());
  results.push_back(bench_submit_yielding_tasks());
  results.push_back(bench_submit_affinity_tasks());
  results.push_back(bench_submit_large_affinity_tasks());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.runtime.scheduler",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
