/**
 *
 *  @file RouteNode.hpp
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
#ifndef VIX_ROUTE_NODE_HPP
#define VIX_ROUTE_NODE_HPP

#include <unordered_map>
#include <memory>
#include <vix/http/IRequestHandler.hpp>

namespace vix::router
{
  struct RouteNode
  {
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> children;
    std::shared_ptr<vix::vhttp::IRequestHandler> handler;
    bool isParam;
    std::string paramName;
    bool heavy;

    RouteNode()
        : children{},
          handler{},
          isParam{false},
          paramName{},
          heavy{false} {}
  };
}

#endif
