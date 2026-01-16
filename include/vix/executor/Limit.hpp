/**
 *
 *  @file Limit.hpp
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
#ifndef VIX_LIMIT_HPP
#define VIX_LIMIT_HPP

#include <vix/executor/IExecutor.hpp>
#include <cstddef>

namespace vix::executor
{
  struct LimitedExecutor
  {
    IExecutor *underlying{nullptr};

    std::size_t maxPending{32};
    bool post(std::function<void()> fn, TaskOptions opt = {})
    {
      return underlying->post(std::move(fn), opt);
    }
  };

  inline LimitedExecutor limit(IExecutor &ex, std::size_t n)
  {
    return LimitedExecutor{&ex, n};
  }

}

#endif
