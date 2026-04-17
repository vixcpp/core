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
#include <utility>

#include <vix/http/IRequestHandler.hpp>

namespace vix::router
{
  /**
   * @brief Internal node of the routing tree used to match HTTP method + path segments.
   *
   * Each node may contain:
   * - static children indexed by the exact segment text
   * - one handler when the route terminates at this node
   * - metadata for parameter nodes such as "{id}"
   * - a heavy flag used by the runtime/router to classify costly routes
   */
  struct RouteNode
  {
    /** @brief Child nodes indexed by static segment or "*" for parameter segments. */
    std::unordered_map<std::string, std::unique_ptr<RouteNode>> children;

    /** @brief Request handler associated with this node (set only for terminal routes). */
    std::shared_ptr<vix::http::IRequestHandler> handler;

    /** @brief True if this node represents a path parameter segment (e.g. "{id}"). */
    bool isParam{false};

    /** @brief Name of the path parameter when isParam is true. */
    std::string paramName{};

    /** @brief True if the route is marked as heavy (CPU/DB intensive). */
    bool heavy{false};

    /** @brief Create an empty routing node. */
    RouteNode() = default;

    /** @brief RouteNode is movable. */
    RouteNode(RouteNode &&) noexcept = default;
    RouteNode &operator=(RouteNode &&) noexcept = default;

    /** @brief RouteNode is non-copyable because it owns children via unique_ptr. */
    RouteNode(const RouteNode &) = delete;
    RouteNode &operator=(const RouteNode &) = delete;

    /** @brief Return true if this node has a terminal handler. */
    [[nodiscard]] bool has_handler() const noexcept
    {
      return static_cast<bool>(handler);
    }

    /** @brief Return true if a child exists for the given segment key. */
    [[nodiscard]] bool has_child(const std::string &segment) const
    {
      return children.find(segment) != children.end();
    }

    /** @brief Return the child for a segment key, or nullptr if absent. */
    [[nodiscard]] RouteNode *find_child(const std::string &segment) noexcept
    {
      auto it = children.find(segment);
      return it == children.end() ? nullptr : it->second.get();
    }

    /** @brief Return the child for a segment key, or nullptr if absent. */
    [[nodiscard]] const RouteNode *find_child(const std::string &segment) const noexcept
    {
      auto it = children.find(segment);
      return it == children.end() ? nullptr : it->second.get();
    }

    /**
     * @brief Get or create a child for the given segment key.
     *
     * This is typically used while inserting routes into the routing tree.
     */
    RouteNode &get_or_create_child(const std::string &segment)
    {
      auto [it, inserted] = children.emplace(segment, nullptr);
      if (inserted || !it->second)
      {
        it->second = std::make_unique<RouteNode>();
      }
      return *it->second;
    }

    /**
     * @brief Mark this node as a parameter node.
     *
     * Example:
     * - segment key stored in parent children may be "*"
     * - paramName stores the extracted parameter name like "id"
     */
    void mark_as_param(std::string name)
    {
      isParam = true;
      paramName = std::move(name);
    }

    /** @brief Assign a terminal request handler to this node. */
    void set_handler(std::shared_ptr<vix::http::IRequestHandler> h) noexcept
    {
      handler = std::move(h);
    }

    /** @brief Mark this node as heavy or non-heavy. */
    void set_heavy(bool value) noexcept
    {
      heavy = value;
    }
  };

} // namespace vix::router

#endif // VIX_ROUTE_NODE_HPP
