/**
 *
 * @file bench_threadpool_vs_runtime.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the
 * License file.
 *
 * Vix.cpp
 *
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <vix/runtime/Runtime.hpp>
#include <vix/experimental/ThreadPoolExecutor.hpp>

namespace
{
  using Clock = std::chrono::steady_clock;
  using Ms = std::chrono::milliseconds;
  using Ns = std::chrono::nanoseconds;

  struct Result
  {
    std::string name;
    std::uint64_t operations;
    double elapsedMs;
    double opsPerSec;
  };

  void print_result(const Result &r)
  {
    std::cout << std::left << std::setw(28) << r.name
              << " | ops=" << std::setw(10) << r.operations
              << " | time_ms=" << std::setw(12) << std::fixed << std::setprecision(3) << r.elapsedMs
              << " | ops/sec=" << std::fixed << std::setprecision(2) << r.opsPerSec
              << '\n';
  }

  template <class F>
  Result measure(const std::string &name, std::uint64_t operations, F &&fn)
  {
    const auto start = Clock::now();
    fn();
    const auto end = Clock::now();

    const double elapsedMs =
        std::chrono::duration_cast<Ns>(end - start).count() / 1'000'000.0;

    const double opsPerSec =
        (elapsedMs <= 0.0) ? 0.0 : (static_cast<double>(operations) * 1000.0 / elapsedMs);

    return Result{name, operations, elapsedMs, opsPerSec};
  }

  bool wait_until(const std::function<bool()> &predicate,
                  Ms timeout = Ms(10000),
                  Ms step = Ms(1))
  {
    const auto deadline = Clock::now() + timeout;

    while (Clock::now() < deadline)
    {
      if (predicate())
      {
        return true;
      }

      std::this_thread::sleep_for(step);
    }

    return predicate();
  }

  void busy_steps(std::uint32_t steps, std::atomic<std::uint64_t> &sink)
  {
    std::uint64_t local = 0;
    for (std::uint32_t i = 0; i < steps; ++i)
    {
      local += static_cast<std::uint64_t>(i ^ (i << 1));
    }
    sink.fetch_add(local, std::memory_order_relaxed);
  }

  Result bench_threadpool_tiny_tasks(std::uint32_t workers, std::uint64_t taskCount)
  {
    std::atomic<std::uint64_t> done{0};
    std::atomic<std::uint64_t> sink{0};

    return measure("threadpool/tiny_tasks", taskCount, [&]()
                   {
                   vix::experimental::ThreadPoolExecutor executor(
                       workers,
                       workers,
                       0);

                   for (std::uint64_t i = 0; i < taskCount; ++i)
                   {
                     const bool ok = executor.post([&done, &sink]()
                                                   {
                                                     busy_steps(8, sink);
                                                     done.fetch_add(1, std::memory_order_relaxed);
                                                   });

                     if (!ok)
                     {
                       std::cerr << "[bench] threadpool post failed\n";
                       std::exit(EXIT_FAILURE);
                     }
                   }

                   executor.wait_idle();

                   const auto completed = done.load(std::memory_order_relaxed);
                   if (completed != taskCount)
                   {
                     std::cerr << "[bench] threadpool tiny tasks mismatch: expected "
                               << taskCount << ", got " << completed << '\n';
                     std::exit(EXIT_FAILURE);
                   } });
  }

  Result bench_runtime_tiny_tasks(std::uint32_t workers, std::uint64_t taskCount)
  {
    std::atomic<std::uint64_t> done{0};
    std::atomic<std::uint64_t> sink{0};

    return measure("runtime/tiny_tasks", taskCount, [&]()
                   {
                     vix::runtime::Runtime runtime(
                         vix::runtime::RuntimeConfig{workers, vix::runtime::BudgetConfig{16}});
                     runtime.start();

                     for (std::uint64_t i = 0; i < taskCount; ++i)
                     {
                       const bool ok = runtime.submit([&done, &sink]() -> vix::runtime::TaskResult
                                                      {
                                                        busy_steps(8, sink);
                                                        done.fetch_add(1, std::memory_order_relaxed);
                                                        return vix::runtime::TaskResult::complete;
                                                      });

                       if (!ok)
                       {
                         std::cerr << "[bench] runtime submit failed\n";
                         std::exit(EXIT_FAILURE);
                       }
                     }

                     const bool ok = wait_until([&done, taskCount]()
                                                { return done.load(std::memory_order_relaxed) == taskCount; });

                     runtime.stop();

                     if (!ok)
                     {
                       std::cerr << "[bench] timeout in runtime tiny tasks\n";
                       std::exit(EXIT_FAILURE);
                     } });
  }

  Result bench_runtime_yielding_tasks(std::uint32_t workers,
                                      std::uint64_t taskCount,
                                      std::uint32_t stepsPerTask)
  {
    std::atomic<std::uint64_t> done{0};
    std::atomic<std::uint64_t> sink{0};

    return measure("runtime/yielding_tasks", taskCount, [&]()
                   {
                     vix::runtime::Runtime runtime(
                         vix::runtime::RuntimeConfig{workers, vix::runtime::BudgetConfig{8}});
                     runtime.start();

                     for (std::uint64_t i = 0; i < taskCount; ++i)
                     {
                       auto state = std::make_shared<std::uint32_t>(0);

                       const bool ok =
                           runtime.submit([state, stepsPerTask, &done, &sink]() mutable -> vix::runtime::TaskResult
                                          {
                                            busy_steps(4, sink);
                                            ++(*state);

                                            if (*state < stepsPerTask)
                                            {
                                              return vix::runtime::TaskResult::yield;
                                            }

                                            done.fetch_add(1, std::memory_order_relaxed);
                                            return vix::runtime::TaskResult::complete;
                                          });

                       if (!ok)
                       {
                         std::cerr << "[bench] runtime submit failed\n";
                         std::exit(EXIT_FAILURE);
                       }
                     }

                     const bool ok = wait_until([&done, taskCount]()
                                                { return done.load(std::memory_order_relaxed) == taskCount; });

                     runtime.stop();

                     if (!ok)
                     {
                       std::cerr << "[bench] timeout in runtime yielding tasks\n";
                       std::exit(EXIT_FAILURE);
                     } });
  }

  Result bench_threadpool_split_tasks(std::uint32_t workers,
                                      std::uint64_t logicalTaskCount,
                                      std::uint32_t stepsPerTask)
  {
    std::atomic<std::uint64_t> done{0};
    std::atomic<std::uint64_t> sink{0};

    return measure("threadpool/split_tasks", logicalTaskCount, [&]()
                   {
                   vix::experimental::ThreadPoolExecutor executor(
                       workers,
                       workers,
                       0);

                   for (std::uint64_t i = 0; i < logicalTaskCount; ++i)
                   {
                     for (std::uint32_t s = 0; s < stepsPerTask; ++s)
                     {
                       const bool ok = executor.post([&done, &sink, s, stepsPerTask]()
                                                     {
                                                       busy_steps(4, sink);

                                                       if (s + 1 == stepsPerTask)
                                                       {
                                                         done.fetch_add(1, std::memory_order_relaxed);
                                                       }
                                                     });

                       if (!ok)
                       {
                         std::cerr << "[bench] threadpool post failed\n";
                         std::exit(EXIT_FAILURE);
                       }
                     }
                   }

                   executor.wait_idle();

                   const auto completed = done.load(std::memory_order_relaxed);
                   if (completed != logicalTaskCount)
                   {
                     std::cerr << "[bench] threadpool split tasks mismatch: expected "
                               << logicalTaskCount << ", got " << completed << '\n';
                     std::exit(EXIT_FAILURE);
                   } });
  }

} // namespace

int main()
{
  const std::uint32_t workers =
      std::max(1u, std::thread::hardware_concurrency());

  const std::uint64_t tinyTaskCount = 100000;
  const std::uint64_t yieldingLogicalTaskCount = 30000;
  const std::uint32_t yieldingSteps = 5;

  std::cout << "Vix benchmark: threadpool vs runtime\n";
  std::cout << "workers=" << workers << "\n\n";

  const auto a = bench_threadpool_tiny_tasks(workers, tinyTaskCount);
  const auto b = bench_runtime_tiny_tasks(workers, tinyTaskCount);
  const auto c = bench_threadpool_split_tasks(workers, yieldingLogicalTaskCount, yieldingSteps);
  const auto d = bench_runtime_yielding_tasks(workers, yieldingLogicalTaskCount, yieldingSteps);

  print_result(a);
  print_result(b);
  print_result(c);
  print_result(d);

  std::cout << "\n";
  std::cout << "Notes:\n";
  std::cout << "- tiny_tasks measures scheduler overhead on short jobs\n";
  std::cout << "- split_tasks vs yielding_tasks approximates resumable work\n";

  return EXIT_SUCCESS;
}
