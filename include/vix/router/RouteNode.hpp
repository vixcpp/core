/**
 * @file RouteNode.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_ROUTE_NODE_HPP
#define VIX_ROUTE_NODE_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include <vix/http/IRequestHandler.hpp>

namespace vix::router
{
  /** @brief Internal node of the routing tree used to match HTTP method + path segments. */
  struct RouteNode
  {
    /** @brief Child nodes indexed by static segment or "*" for parameter segments. */
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> children;

    /** @brief Request handler associated with this node (set only for terminal routes). */
    std::shared_ptr<vix::vhttp::IRequestHandler> handler;

    /** @brief True if this node represents a path parameter segment (e.g. "{id}"). */
    bool isParam;

    /** @brief Name of the path parameter when isParam is true. */
    std::string paramName;

    /** @brief True if the route is marked as heavy (CPU/DB intensive). */
    bool heavy;

    /** @brief Create an empty routing node. */
    RouteNode()
        : children{},
          handler{},
          isParam{false},
          paramName{},
          heavy{false}
    {
    }
  };

} // namespace vix::router

#endif // VIX_ROUTE_NODE_HPP
