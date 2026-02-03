/**
 *
 * @file TaskGuard.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_TASK_GUARD_HPP
#define VIX_TASK_GUARD_HPP

#include <atomic>
#include <type_traits>

namespace vix::threadpool
{

  /**
   * @brief RAII guard for tracking active tasks.
   *
   * Increments a shared atomic counter on construction and decrements it on
   * destruction.
   */
  template <class T>
  struct TaskGuard
  {
    static_assert(std::is_integral_v<T>, "TaskGuard requires an integral counter type");

    /** @brief Reference to the shared task counter. */
    std::atomic<T> &counter;

    /** @brief Increment the counter on construction. */
    explicit TaskGuard(std::atomic<T> &c) : counter(c)
    {
      counter.fetch_add(1, std::memory_order_relaxed);
    }

    /** @brief Decrement the counter on destruction. */
    ~TaskGuard()
    {
      counter.fetch_sub(1, std::memory_order_relaxed);
    }
  };

  /** @brief Deduction guide for TaskGuard. */
  template <class T>
  TaskGuard(std::atomic<T> &) -> TaskGuard<T>;

} // namespace vix::threadpool

#endif // VIX_TASK_GUARD_HPP
