/**
 *
 * @file Task.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_TASK_HPP
#define VIX_TASK_HPP

#include <functional>
#include <cstdint>

namespace vix::threadpool
{

  /**
   * @brief Unit of work executed by the thread pool.
   *
   * Holds a callable, its priority, and a sequence number used for ordering.
   */
  struct Task
  {
    std::function<void()> func;
    int priority;
    std::uint64_t seq;

    /** @brief Construct a task with function, priority, and sequence number. */
    Task(std::function<void()> f, int p, std::uint64_t s)
        : func(std::move(f)), priority(p), seq(s) {}

    /** @brief Construct an empty task. */
    Task()
        : func(nullptr), priority(0), seq(0) {}
  };

} // namespace vix::threadpool

#endif // VIX_TASK_HPP
