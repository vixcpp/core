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
#include <memory>
#include <string>
#include <thread>

#include <vix/runtime/Runtime.hpp>

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

  void print_result(const Result &result)
  {
    std::cout << std::left << std::setw(28) << result.name
              << " | ops=" << std::setw(10) << result.operations
              << " | time_ms=" << std::setw(12) << std::fixed << std::setprecision(3) << result.elapsedMs
              << " | ops/sec=" << std::fixed << std::setprecision(2) << result.opsPerSec
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

  Result bench_runtime_cpu_tasks(std::uint32_t workers, std::uint64_t taskCount)
  {
    std::atomic<std::uint64_t> done{0};
    std::atomic<std::uint64_t> sink{0};

    return measure("runtime/cpu_tasks", taskCount, [&]()
                   {
                     vix::runtime::Runtime runtime(
                         vix::runtime::RuntimeConfig{workers, vix::runtime::BudgetConfig{32}});

                     runtime.start();

                     for (std::uint64_t i = 0; i < taskCount; ++i)
                     {
                       const bool ok = runtime.submit([&done, &sink]() -> vix::runtime::TaskResult
                                                      {
                                                        busy_steps(256, sink);
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
                       std::cerr << "[bench] timeout in runtime cpu tasks\n";
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
  const std::uint64_t cpuTaskCount = 50000;
  const std::uint32_t yieldingSteps = 5;

  std::cout << "Vix benchmark: runtime scheduler\n";
  std::cout << "workers=" << workers << "\n\n";

  const auto tiny = bench_runtime_tiny_tasks(workers, tinyTaskCount);
  const auto yielding = bench_runtime_yielding_tasks(
      workers,
      yieldingLogicalTaskCount,
      yieldingSteps);
  const auto cpu = bench_runtime_cpu_tasks(workers, cpuTaskCount);

  print_result(tiny);
  print_result(yielding);
  print_result(cpu);

  std::cout << "\n";
  std::cout << "Notes:\n";
  std::cout << "- tiny_tasks measures runtime scheduler overhead on short jobs\n";
  std::cout << "- yielding_tasks measures resumable work with cooperative yielding\n";
  std::cout << "- cpu_tasks measures heavier runtime task execution\n";

  return EXIT_SUCCESS;
}
