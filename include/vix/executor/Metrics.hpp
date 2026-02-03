/**
 *
 * @file Metrics.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_METRICS_HPP
#define VIX_METRICS_HPP

#include <cstdint>

namespace vix::executor
{

  /**
   * @brief Executor metrics snapshot.
   */
  struct Metrics
  {
    std::uint64_t pending{0};
    std::uint64_t active{0};
    std::uint64_t timed_out{0};
  };

} // namespace vix::executor

#endif // VIX_METRICS_HPP
