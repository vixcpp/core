/**
 *
 * @file Stand.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_STAND_HPP
#define VIX_STAND_HPP

#include <vix/executor/IExecutor.hpp>

namespace vix::executor
{

  /**
   * @brief Lightweight executor wrapper.
   */
  struct Stand
  {
    /** @brief Underlying executor. */
    IExecutor *underlying{nullptr};

    /** @brief Construct a stand wrapper from an executor. */
    explicit Stand(IExecutor &ex)
        : underlying(&ex) {}

    /** @brief Forward task posting to the underlying executor. */
    bool post(std::function<void()> fn, TaskOptions opt = {})
    {
      return underlying->post(std::move(fn), opt);
    }
  };

  /**
   * @brief Create a stand wrapper for an executor.
   */
  inline Stand makeStand(IExecutor &ex)
  {
    return Stand(ex);
  }

} // namespace vix::executor

#endif // VIX_STAND_HPP
