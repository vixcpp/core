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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
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
} // namespace

TEST(VixRuntimeTest, ExecutesSubmittedTasks)
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

    ASSERT_TRUE(ok);
  }

  const bool done = wait_until(
      [&completed]()
      {
        return completed.load(std::memory_order_relaxed) == taskCount;
      });

  runtime.stop();

  ASSERT_TRUE(done);
  EXPECT_EQ(completed.load(std::memory_order_relaxed), taskCount);
}

TEST(VixRuntimeTest, ReschedulesYieldingTaskUntilCompletion)
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

  ASSERT_TRUE(ok);

  const bool done = wait_until(
      [&doneCount]()
      {
        return doneCount.load(std::memory_order_relaxed) == 1;
      });

  runtime.stop();

  ASSERT_TRUE(done);
  EXPECT_EQ(stepCount.load(std::memory_order_relaxed), 5);
  EXPECT_EQ(doneCount.load(std::memory_order_relaxed), 1);
}

TEST(VixRuntimeTest, GeneratesUniqueTaskIds)
{
  using namespace vix::runtime;

  Runtime runtime(RuntimeConfig{1, BudgetConfig{4}});

  const TaskId a = runtime.next_task_id();
  const TaskId b = runtime.next_task_id();
  const TaskId c = runtime.next_task_id();

  EXPECT_NE(a, 0u);
  EXPECT_EQ(b, a + 1);
  EXPECT_EQ(c, b + 1);
}
