/**
 *
 * @file TaskOptions.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_TASK_OPTIONS_HPP
#define VIX_TASK_OPTIONS_HPP

#include <chrono>

namespace vix::executor
{

  /**
   * @brief Execution options associated with a task.
   */
  struct TaskOptions
  {
    int priority = 0;
    std::chrono::milliseconds timeout{0};
    std::chrono::milliseconds deadline{0};
    bool may_block = false;
  };

} // namespace vix::executor

#endif // VIX_TASK_OPTIONS_HPP
