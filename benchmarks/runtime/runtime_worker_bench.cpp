/**
 *
 * @file runtime_worker_bench.cpp
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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
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
  using Worker = vix::runtime::Worker;

  using namespace std::chrono_literals;

  constexpr std::uint64_t kWorkerTasks = 100'000;
  constexpr std::uint64_t kYieldingTasks = 25'000;
  constexpr std::uint64_t kBatchIterations = 2'000;
  constexpr std::size_t kBatchSize = 64;
  constexpr std::uint64_t kStartStopIterations = 50;

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
      std::atomic<std::uint64_t> &calls,
      std::atomic<std::uint64_t> &sink,
      std::uint32_t complete_after)
  {
    return Task{
        id,
        [&completed, &calls, &sink, complete_after]()
        {
          const std::uint64_t n =
              calls.fetch_add(1u, std::memory_order_relaxed) + 1u;

          sink.fetch_add(1u, std::memory_order_relaxed);

          if ((n % complete_after) != 0u)
          {
            return TaskResult::yield;
          }

          completed.fetch_add(1u, std::memory_order_relaxed);

          return TaskResult::complete;
        }};
  }

  static std::vector<Task> make_task_batch(
      std::size_t count,
      TaskId base_id,
      std::atomic<std::uint64_t> &completed,
      std::atomic<std::uint64_t> &sink)
  {
    std::vector<Task> tasks;
    tasks.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
      tasks.push_back(
          make_complete_task(
              base_id + static_cast<TaskId>(i),
              completed,
              sink,
              static_cast<std::uint32_t>(i % 4u)));
    }

    return tasks;
  }

  static void stop_and_join(Worker &worker)
  {
    worker.stop();
    worker.join();
  }

  static vix::bench::BenchmarkResult bench_start_stop()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.worker/start_stop",
            kStartStopIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kStartStopIterations; ++i)
              {
                Worker worker{0u, BudgetConfig{8u}};

                worker.start();

                sink += static_cast<std::uint64_t>(worker.running());

                worker.stop();
                worker.join();

                sink += static_cast<std::uint64_t>(!worker.running());
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
            "runtime.worker/submit_complete_tasks",
            kWorkerTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Worker worker{0u, BudgetConfig{16u}};

              worker.start();

              for (std::uint64_t i = 0; i < kWorkerTasks; ++i)
              {
                const bool accepted =
                    worker.submit(
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
                        return completed.load(std::memory_order_relaxed) == kWorkerTasks;
                      });

              stop_and_join(worker);

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kWorkerTasks);
              assert(worker.executed_tasks() >= kWorkerTasks);
              assert(worker.failed_tasks() == 0u);

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
            "runtime.worker/submit_cpu_tasks",
            kWorkerTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Worker worker{0u, BudgetConfig{32u}};

              worker.start();

              for (std::uint64_t i = 0; i < kWorkerTasks; ++i)
              {
                const bool accepted =
                    worker.submit(
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
                        return completed.load(std::memory_order_relaxed) == kWorkerTasks;
                      });

              stop_and_join(worker);

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kWorkerTasks);
              assert(worker.executed_tasks() >= kWorkerTasks);
              assert(worker.failed_tasks() == 0u);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_batch_complete_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> sink{0};

    constexpr std::uint64_t operations =
        kBatchIterations * static_cast<std::uint64_t>(kBatchSize);

    auto result =
        vix::bench::run(
            "runtime.worker/submit_batch_complete_tasks",
            operations,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Worker worker{0u, BudgetConfig{16u}};

              worker.start();

              for (std::uint64_t i = 0; i < kBatchIterations; ++i)
              {
                std::vector<Task> tasks =
                    make_task_batch(
                        kBatchSize,
                        static_cast<TaskId>((i * kBatchSize) + 1u),
                        completed,
                        sink);

                const std::size_t accepted =
                    worker.submit_batch(std::move(tasks));

                if (accepted != kBatchSize)
                {
                  std::abort();
                }
              }

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == operations;
                      });

              stop_and_join(worker);

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == operations);
              assert(worker.executed_tasks() >= operations);
              assert(worker.failed_tasks() == 0u);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_submit_yielding_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> sink{0};

    constexpr std::uint32_t complete_after = 4u;

    auto result =
        vix::bench::run(
            "runtime.worker/submit_yielding_tasks",
            kYieldingTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              calls.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Worker worker{0u, BudgetConfig{8u}};

              worker.start();

              for (std::uint64_t i = 0; i < kYieldingTasks; ++i)
              {
                const bool accepted =
                    worker.submit(
                        make_yielding_task(
                            static_cast<TaskId>(i + 1u),
                            completed,
                            calls,
                            sink,
                            complete_after));

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

              stop_and_join(worker);

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kYieldingTasks);
              assert(calls.load(std::memory_order_relaxed) >= kYieldingTasks);
              assert(worker.yielded_tasks() >= kYieldingTasks);
              assert(worker.failed_tasks() == 0u);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) >= kYieldingTasks);

    return result;
  }

  static vix::bench::BenchmarkResult bench_steal_callback_complete_tasks()
  {
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> provided{0};
    std::atomic<std::uint64_t> callback_calls{0};
    std::atomic<std::uint64_t> sink{0};

    auto result =
        vix::bench::run(
            "runtime.worker/steal_callback_complete_tasks",
            kWorkerTasks,
            [&]()
            {
              completed.store(0u, std::memory_order_relaxed);
              provided.store(0u, std::memory_order_relaxed);
              callback_calls.store(0u, std::memory_order_relaxed);
              sink.store(0u, std::memory_order_relaxed);

              Worker worker{3u, BudgetConfig{16u}};

              worker.set_steal_callback(
                  [&completed, &provided, &callback_calls, &sink](
                      std::uint32_t worker_id) -> std::optional<Task>
                  {
                    callback_calls.fetch_add(1u, std::memory_order_relaxed);

                    if (worker_id != 3u)
                    {
                      return std::nullopt;
                    }

                    const std::uint64_t current =
                        provided.fetch_add(1u, std::memory_order_relaxed);

                    if (current >= kWorkerTasks)
                    {
                      return std::nullopt;
                    }

                    return make_complete_task(
                        static_cast<TaskId>(current + 1u),
                        completed,
                        sink);
                  });

              worker.start();

              const bool done =
                  wait_until(
                      [&completed]()
                      {
                        return completed.load(std::memory_order_relaxed) == kWorkerTasks;
                      });

              stop_and_join(worker);

              assert(done);
              assert(completed.load(std::memory_order_relaxed) == kWorkerTasks);
              assert(worker.stolen_tasks() >= kWorkerTasks);
              assert(worker.executed_tasks() >= kWorkerTasks);
              assert(callback_calls.load(std::memory_order_relaxed) >= kWorkerTasks);

              vix::bench::do_not_optimize(sink.load(std::memory_order_relaxed));
            });

    assert(sink.load(std::memory_order_relaxed) > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_try_steal_empty()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.worker/try_steal_empty",
            kWorkerTasks,
            [&]()
            {
              Worker worker{0u};

              for (std::uint64_t i = 0; i < kWorkerTasks; ++i)
              {
                auto task = worker.try_steal();

                sink += static_cast<std::uint64_t>(!task.has_value());
              }

              assert(worker.empty());
              assert(worker.size() == 0u);

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_start_stop());
  results.push_back(bench_submit_complete_tasks());
  results.push_back(bench_submit_cpu_tasks());
  results.push_back(bench_submit_batch_complete_tasks());
  results.push_back(bench_submit_yielding_tasks());
  results.push_back(bench_steal_callback_complete_tasks());
  results.push_back(bench_try_steal_empty());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.runtime.worker",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
