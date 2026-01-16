/**
 *
 *  @file Task.hpp
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
#ifndef VIX_TASK_HPP
#define VIX_TASK_HPP

#include <functional>
#include <cstdint>

namespace vix::threadpool
{
  struct Task
  {
    std::function<void()> func;
    int priority;
    std::uint64_t seq;

    Task(std::function<void()> f, int p, std::uint64_t s)
        : func(std::move(f)), priority(p), seq(s) {}
    Task()
        : func(nullptr), priority(0), seq(0) {}
  };

}

#endif
