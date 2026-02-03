/**
 *
 * @file Limit.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_LIMIT_HPP
#define VIX_LIMIT_HPP

#include <vix/executor/IExecutor.hpp>
#include <cstddef>

namespace vix::executor
{

  /**
   * @brief Executor wrapper that enforces a pending task limit.
   */
  struct LimitedExecutor
  {
    /** @brief Underlying executor. */
    IExecutor *underlying{nullptr};

    /** @brief Maximum number of pending tasks allowed. */
    std::size_t maxPending{32};

    /** @brief Post a task to the underlying executor. */
    bool post(std::function<void()> fn, TaskOptions opt = {})
    {
      return underlying->post(std::move(fn), opt);
    }
  };

  /**
   * @brief Create a limited view over an executor.
   *
   * @param ex Underlying executor.
   * @param n Maximum number of pending tasks.
   * @return LimitedExecutor wrapper.
   */
  inline LimitedExecutor limit(IExecutor &ex, std::size_t n)
  {
    return LimitedExecutor{&ex, n};
  }

} // namespace vix::executor

#endif // VIX_LIMIT_HPP
