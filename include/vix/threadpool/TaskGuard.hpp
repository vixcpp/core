/**
 *
 *  @file TaskGuard.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#ifndef VIX_TASK_GUARD_HPP
#define VIX_TASK_GUARD_HPP

#include <atomic>

namespace vix::threadpool
{
  template <class T>
  struct TaskGuard
  {
    static_assert(std::is_integral_v<T>, "TaskGuard requires an integral counter type");

    std::atomic<T> &counter;

    explicit TaskGuard(std::atomic<T> &c) : counter(c)
    {
      counter.fetch_add(1, std::memory_order_relaxed);
    }

    ~TaskGuard()
    {
      counter.fetch_sub(1, std::memory_order_relaxed);
    }
  };

  template <class T>
  TaskGuard(std::atomic<T> &) -> TaskGuard<T>;
}

#endif
