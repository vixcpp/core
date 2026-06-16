/**
 *
 * @file runtime_queue_bench.cpp
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <vix/runtime/RunQueue.hpp>
#include <vix/runtime/Task.hpp>

namespace
{
  using RunQueue = vix::runtime::RunQueue;
  using Task = vix::runtime::Task;
  using TaskFn = vix::runtime::TaskFn;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;

  constexpr std::uint64_t kQueueIterations = 200'000;
  constexpr std::uint64_t kBatchIterations = 20'000;
  constexpr std::size_t kBatchSize = 64;

  static Task make_task(
      TaskId id,
      std::uint64_t &sink,
      std::uint32_t affinity = 0)
  {
    return Task{
        id,
        [&sink]()
        {
          ++sink;
          return TaskResult::complete;
        },
        affinity};
  }

  static Task make_yielded_task(
      TaskId id,
      std::uint64_t &sink)
  {
    Task task{
        id,
        [&sink]()
        {
          ++sink;
          return TaskResult::yield;
        }};

    task.state = TaskState::yielded;

    return task;
  }

  static std::vector<Task> make_task_batch(
      std::size_t count,
      TaskId base_id,
      std::uint64_t &sink)
  {
    std::vector<Task> tasks;
    tasks.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
      tasks.push_back(
          make_task(
              base_id + static_cast<TaskId>(i),
              sink,
              static_cast<std::uint32_t>(i % 4u)));
    }

    return tasks;
  }

  static vix::bench::BenchmarkResult bench_push_clear()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/push_clear",
            kQueueIterations,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                const bool accepted =
                    queue.push(
                        make_task(
                            static_cast<TaskId>(i + 1u),
                            sink));

                sink += static_cast<std::uint64_t>(accepted);
              }

              sink += queue.clear();

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_push_pop()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/push_pop",
            kQueueIterations * 2u,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                const bool accepted =
                    queue.push(
                        make_task(
                            static_cast<TaskId>(i + 1u),
                            sink));

                sink += static_cast<std::uint64_t>(accepted);
              }

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                auto task = queue.try_pop();

                assert(task.has_value());

                sink += task->id;
              }

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_push_steal()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/push_steal",
            kQueueIterations * 2u,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                const bool accepted =
                    queue.push(
                        make_task(
                            static_cast<TaskId>(i + 1u),
                            sink));

                sink += static_cast<std::uint64_t>(accepted);
              }

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                auto task = queue.try_steal();

                assert(task.has_value());

                sink += task->id;
              }

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_front_back_size()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/front_back_size",
            kQueueIterations * 3u,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kBatchSize; ++i)
              {
                const bool accepted =
                    queue.push(
                        make_task(
                            static_cast<TaskId>(i + 1u),
                            sink));

                sink += static_cast<std::uint64_t>(accepted);
              }

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                auto front = queue.front();
                auto back = queue.back();

                assert(front.has_value());
                assert(back.has_value());

                sink += front->id;
                sink += back->id;
                sink += queue.size();
              }

              sink += queue.clear();

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_push_yielded_normalize()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/push_yielded_normalize",
            kQueueIterations,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                const bool accepted =
                    queue.push(
                        make_yielded_task(
                            static_cast<TaskId>(i + 1u),
                            sink));

                sink += static_cast<std::uint64_t>(accepted);
              }

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                auto task = queue.try_pop();

                assert(task.has_value());
                assert(task->state == TaskState::ready);

                sink += task->id;
              }

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_push_batch()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/push_batch",
            kBatchIterations * kBatchSize,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kBatchIterations; ++i)
              {
                std::vector<Task> tasks =
                    make_task_batch(
                        kBatchSize,
                        static_cast<TaskId>((i * kBatchSize) + 1u),
                        sink);

                const std::size_t accepted =
                    queue.push_batch(std::move(tasks));

                assert(accepted == kBatchSize);

                sink += accepted;
              }

              sink += queue.clear();

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_push_batch_pop_batch()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/push_batch_pop_batch",
            kBatchIterations * kBatchSize * 2u,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kBatchIterations; ++i)
              {
                std::vector<Task> tasks =
                    make_task_batch(
                        kBatchSize,
                        static_cast<TaskId>((i * kBatchSize) + 1u),
                        sink);

                const std::size_t accepted =
                    queue.push_batch(std::move(tasks));

                assert(accepted == kBatchSize);

                std::vector<Task> popped =
                    queue.try_pop_batch(kBatchSize);

                assert(popped.size() == kBatchSize);

                for (const Task &task : popped)
                {
                  sink += task.id;
                }

                assert(queue.empty());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_try_pop_empty()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/try_pop_empty",
            kQueueIterations,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                auto task = queue.try_pop();

                sink += static_cast<std::uint64_t>(!task.has_value());
              }

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_try_steal_empty()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/try_steal_empty",
            kQueueIterations,
            [&]()
            {
              RunQueue queue;

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                auto task = queue.try_steal();

                sink += static_cast<std::uint64_t>(!task.has_value());
              }

              assert(queue.empty());

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_swap_queues()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.queue/swap",
            kQueueIterations,
            [&]()
            {
              RunQueue first;
              RunQueue second;

              for (std::uint64_t i = 0; i < kBatchSize; ++i)
              {
                assert(first.push(make_task(static_cast<TaskId>(i + 1u), sink)));
                assert(second.push(make_task(static_cast<TaskId>(i + 1000u), sink)));
              }

              for (std::uint64_t i = 0; i < kQueueIterations; ++i)
              {
                first.swap(second);

                sink += first.size();
                sink += second.size();
              }

              sink += first.clear();
              sink += second.clear();

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_push_clear());
  results.push_back(bench_push_pop());
  results.push_back(bench_push_steal());
  results.push_back(bench_front_back_size());
  results.push_back(bench_push_yielded_normalize());
  results.push_back(bench_push_batch());
  results.push_back(bench_push_batch_pop_batch());
  results.push_back(bench_try_pop_empty());
  results.push_back(bench_try_steal_empty());
  results.push_back(bench_swap_queues());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.runtime.queue",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
