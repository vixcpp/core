/**
 *
 * @file runtime_task_bench.cpp
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
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <vix/runtime/Task.hpp>

namespace
{
  using Task = vix::runtime::Task;
  using TaskFn = vix::runtime::TaskFn;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;

  constexpr std::uint64_t kTaskIterations = 1'000'000;
  constexpr std::uint64_t kStateIterations = 1'000'000;
  constexpr std::uint64_t kExecuteIterations = 1'000'000;

  static TaskFn make_complete_fn(std::uint64_t &sink)
  {
    return [&sink]() -> TaskResult
    {
      ++sink;
      return TaskResult::complete;
    };
  }

  static vix::bench::BenchmarkResult bench_default_construct_task()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.task/default_construct",
            kTaskIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kTaskIterations; ++i)
              {
                Task task;

                sink += task.id;
                sink += static_cast<std::uint64_t>(task.state == TaskState::ready);
                sink += static_cast<std::uint64_t>(task.affinity);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_valid_task()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.task/construct_valid",
            kTaskIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kTaskIterations; ++i)
              {
                Task task{
                    static_cast<TaskId>(i + 1u),
                    make_complete_fn(sink),
                    static_cast<std::uint32_t>(i % 4u)};

                sink += static_cast<std::uint64_t>(task.valid());
                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.affinity);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_task_state_queries()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.task/state_queries",
            kStateIterations * 6u,
            [&]()
            {
              Task task{
                  1u,
                  make_complete_fn(sink)};

              for (std::uint64_t i = 0; i < kStateIterations; ++i)
              {
                task.state = TaskState::ready;

                sink += static_cast<std::uint64_t>(task.valid());
                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.done());
                sink += static_cast<std::uint64_t>(task.running());
                sink += static_cast<std::uint64_t>(task.yielded());

                task.state = TaskState::running;

                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.done());
                sink += static_cast<std::uint64_t>(task.running());
                sink += static_cast<std::uint64_t>(task.yielded());

                task.state = TaskState::yielded;

                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.done());
                sink += static_cast<std::uint64_t>(task.running());
                sink += static_cast<std::uint64_t>(task.yielded());

                task.state = TaskState::completed;

                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.done());
                sink += static_cast<std::uint64_t>(task.running());
                sink += static_cast<std::uint64_t>(task.yielded());

                task.state = TaskState::failed;

                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.done());
                sink += static_cast<std::uint64_t>(task.running());
                sink += static_cast<std::uint64_t>(task.yielded());

                task.state = TaskState::cancelled;

                sink += static_cast<std::uint64_t>(task.schedulable());
                sink += static_cast<std::uint64_t>(task.done());
                sink += static_cast<std::uint64_t>(task.running());
                sink += static_cast<std::uint64_t>(task.yielded());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_mark_ready()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.task/mark_ready",
            kStateIterations * 3u,
            [&]()
            {
              Task task{
                  1u,
                  make_complete_fn(sink)};

              for (std::uint64_t i = 0; i < kStateIterations; ++i)
              {
                task.state = TaskState::running;
                task.mark_ready();
                sink += static_cast<std::uint64_t>(task.state == TaskState::ready);

                task.state = TaskState::yielded;
                task.mark_ready();
                sink += static_cast<std::uint64_t>(task.state == TaskState::ready);

                task.state = TaskState::ready;
                task.mark_ready();
                sink += static_cast<std::uint64_t>(task.state == TaskState::ready);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_cancel()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.task/cancel",
            kStateIterations * 3u,
            [&]()
            {
              Task task{
                  1u,
                  make_complete_fn(sink)};

              for (std::uint64_t i = 0; i < kStateIterations; ++i)
              {
                task.state = TaskState::ready;
                task.cancel();
                sink += static_cast<std::uint64_t>(task.state == TaskState::cancelled);

                task.state = TaskState::running;
                task.cancel();
                sink += static_cast<std::uint64_t>(task.state == TaskState::cancelled);

                task.state = TaskState::yielded;
                task.cancel();
                sink += static_cast<std::uint64_t>(task.state == TaskState::cancelled);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

  static vix::bench::BenchmarkResult bench_execute_complete_task_fn()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "runtime.task/execute_complete_fn",
            kExecuteIterations,
            [&]()
            {
              Task task{
                  1u,
                  make_complete_fn(sink)};

              for (std::uint64_t i = 0; i < kExecuteIterations; ++i)
              {
                const TaskResult task_result = task.fn();

                sink += static_cast<std::uint64_t>(task_result == TaskResult::complete);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_default_construct_task());
  results.push_back(bench_construct_valid_task());
  results.push_back(bench_task_state_queries());
  results.push_back(bench_mark_ready());
  results.push_back(bench_cancel());
  results.push_back(bench_execute_complete_task_fn());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.runtime.task",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
