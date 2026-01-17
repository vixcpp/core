/**
 *
 *  @file TaskCmp.hpp
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
#ifndef VIX_TASK_COMP_HPP
#define VIX_TASK_COMP_HPP

#include <vix/threadpool/Task.hpp>

namespace vix::threadpool
{
  struct TaskCmp
  {
    bool operator()(const Task &a, const Task &b) const noexcept
    {
      if (a.priority != b.priority)
      {
        return a.priority < b.priority;
      }
      return a.seq > b.seq;
    }
  };

}

#endif
