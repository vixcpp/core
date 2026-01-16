/**
 *
 *  @file Stand.hpp
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
#ifndef VIX_STAND_HPP
#define VIX_STAND_HPP

#include <vix/executor/IExecutor.hpp>

namespace vix::executor
{
  struct Stand
  {
    IExecutor *underlying{nullptr};
    explicit Stand(IExecutor &ex)
        : underlying(&ex) {}

    bool post(std::function<void()> fn, TaskOptions opt = {})
    {
      return underlying->post(std::move(fn), opt);
    }
  };

  inline Stand makeStand(IExecutor &ex)
  {
    return Stand(ex);
  }
}

#endif
