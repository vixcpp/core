/**
 *
 * @file test_runtime.cpp
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
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>

#include <vix/runtime/Runtime.hpp>

namespace
{
  bool wait_until(const std::function<bool()> &predicate,
                  std::chrono::milliseconds timeout = std::chrono::milliseconds(2000),
                  std::chrono::milliseconds step = std::chrono::milliseconds(5))
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
      if (predicate())
      {
        return true;
      }

      std::this_thread::sleep_for(step);
    }

    return predicate();
  }

  int fail(const char *message)
  {
    std::cerr << "[runtime_test] FAIL: " << message << '\n';
    return EXIT_FAILURE;
  }

  int test_executes_submitted_tasks()
  {
    using namespace vix::runtime;

    Runtime runtime(RuntimeConfig{2, BudgetConfig{8}});
    runtime.start();

    std::atomic<int> completed{0};
    constexpr int taskCount = 32;

    for (int i = 0; i < taskCount; ++i)
    {
      const bool ok = runtime.submit([&completed]() -> TaskResult
                                     {
                                       completed.fetch_add(1, std::memory_order_relaxed);
                                       return TaskResult::complete; });

      if (!ok)
      {
        runtime.stop();
        return fail("submit() failed in test_executes_submitted_tasks");
      }
    }

    const bool done = wait_until(
        [&completed]()
        {
          return completed.load(std::memory_order_relaxed) == taskCount;
        });

    runtime.stop();

    if (!done)
    {
      return fail("tasks did not complete before timeout");
    }

    if (completed.load(std::memory_order_relaxed) != taskCount)
    {
      return fail("completed task count mismatch");
    }

    return EXIT_SUCCESS;
  }

  int test_reschedules_yielding_task_until_completion()
  {
    using namespace vix::runtime;

    Runtime runtime(RuntimeConfig{2, BudgetConfig{4}});
    runtime.start();

    std::atomic<int> stepCount{0};
    std::atomic<int> doneCount{0};

    const bool ok = runtime.submit([&stepCount, &doneCount]() mutable -> TaskResult
                                   {
                                     const int step =
                                         stepCount.fetch_add(1, std::memory_order_relaxed) + 1;

                                     if (step < 5)
                                     {
                                       return TaskResult::yield;
                                     }

                                     doneCount.fetch_add(1, std::memory_order_relaxed);
                                     return TaskResult::complete; });

    if (!ok)
    {
      runtime.stop();
      return fail("submit() failed in yielding task test");
    }

    const bool done = wait_until(
        [&doneCount]()
        {
          return doneCount.load(std::memory_order_relaxed) == 1;
        });

    runtime.stop();

    if (!done)
    {
      return fail("yielding task did not complete before timeout");
    }

    if (stepCount.load(std::memory_order_relaxed) != 5)
    {
      return fail("yielding task step count mismatch");
    }

    if (doneCount.load(std::memory_order_relaxed) != 1)
    {
      return fail("yielding task completion count mismatch");
    }

    return EXIT_SUCCESS;
  }

  int test_generates_unique_task_ids()
  {
    using namespace vix::runtime;

    Runtime runtime(RuntimeConfig{1, BudgetConfig{4}});

    const TaskId a = runtime.next_task_id();
    const TaskId b = runtime.next_task_id();
    const TaskId c = runtime.next_task_id();

    if (a == 0u)
    {
      return fail("first task id must not be zero");
    }

    if (b != a + 1)
    {
      return fail("second task id is not sequential");
    }

    if (c != b + 1)
    {
      return fail("third task id is not sequential");
    }

    return EXIT_SUCCESS;
  }

} // namespace

int main()
{
  if (const int rc = test_executes_submitted_tasks(); rc != EXIT_SUCCESS)
  {
    return rc;
  }

  if (const int rc = test_reschedules_yielding_task_until_completion(); rc != EXIT_SUCCESS)
  {
    return rc;
  }

  if (const int rc = test_generates_unique_task_ids(); rc != EXIT_SUCCESS)
  {
    return rc;
  }

  std::cout << "[runtime_test] PASS\n";
  return EXIT_SUCCESS;
}
