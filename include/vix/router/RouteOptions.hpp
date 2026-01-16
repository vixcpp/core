/**
 *
 *  @file RouteOptions.hpp
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
#ifndef VIX_ROUTE_OPTIONS_HPP
#define VIX_ROUTE_OPTIONS_HPP

namespace vix::router
{
  struct RouteOptions
  {
    bool heavy{false}; // DB/CPU heavy => executor
  };
}

#endif
