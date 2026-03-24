/**
 * @file Router.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <vix/async/core/task.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/http/Status.hpp>
#include <vix/router/RouteDoc.hpp>
#include <vix/router/RouteNode.hpp>
#include <vix/router/RouteOptions.hpp>

namespace vix::router
{
  using vix::async::core::task;

  /**
   * @brief Lightweight route matcher/dispatcher backed by a route tree (method + path),
   * with optional OpenAPI metadata.
   *
   * This native Vix router does not depend on Boost.Beast.
   */
  class Router
  {
  public:
    /**
     * @brief Custom handler called when no route matches a request.
     */
    using NotFoundHandler = std::function<task<void>(
        const vix::vhttp::Request &,
        vix::vhttp::Response &)>;

    /**
     * @brief Metadata for one registered route (used for docs/OpenAPI and runtime checks).
     */
    struct RouteRecord
    {
      std::string method{};
      std::string path{};
      bool heavy{false};
      RouteDoc doc{};
    };

    /**
     * @brief Create an empty router with an initialized route tree.
     */
    Router()
        : root_{std::make_unique<RouteNode>()},
          notFound_{},
          registered_routes_{}
    {
    }

    /**
     * @brief Set a custom not-found handler.
     */
    void setNotFoundHandler(NotFoundHandler h)
    {
      notFound_ = std::move(h);
    }

    /**
     * @brief Register a route handler for (method, path).
     */
    void add_route(
        std::string method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler)
    {
      add_route(std::move(method), path, std::move(handler), RouteOptions{}, RouteDoc{});
    }

    /**
     * @brief Register a route handler for (method, path) with options.
     */
    void add_route(
        std::string method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler,
        RouteOptions opt)
    {
      add_route(std::move(method), path, std::move(handler), std::move(opt), RouteDoc{});
    }

    /**
     * @brief Register a route handler for (method, path) with options and documentation metadata.
     */
    void add_route(
        std::string method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler,
        RouteOptions opt,
        RouteDoc doc)
    {
      const std::string normalized_method = normalize_method(method);
      const std::string normalized_path = normalize_path(path);
      const std::string full_path = normalized_method + normalized_path;

      auto *node = root_.get();
      std::size_t start = 0;

      while (start < full_path.size())
      {
        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        const std::string segment = full_path.substr(start, end - start);

        const bool is_param =
            !segment.empty() && segment.front() == '{' && segment.back() == '}';

        const std::string key = is_param ? "*" : segment;

        RouteNode &child = node->get_or_create_child(key);
        if (is_param)
        {
          child.mark_as_param(segment.substr(1, segment.size() - 2));
        }

        node = &child;
        start = end + 1;
      }

      node->set_handler(std::move(handler));
      node->set_heavy(opt.heavy);

      if (opt.heavy)
        doc.x["x-vix-heavy"] = true;

      registered_routes_.push_back(RouteRecord{
          normalized_method,
          normalized_path,
          opt.heavy,
          std::move(doc)});
    }

    /**
     * @brief Dispatch a request to the matching handler.
     *
     * Returns true once handled, including 404 responses.
     */
    task<bool> handle_request(
        const vix::vhttp::Request &req,
        vix::vhttp::Response &res)
    {
      const std::string method = normalize_method(req.method());
      const bool is_head = (method == "HEAD");

      if (method == "OPTIONS")
      {
        const std::string target = strip_query(req.target());
        if (!has_route("OPTIONS", target))
        {
          res.set_status(vix::vhttp::NO_CONTENT);
          res.set_body("");
          res.set_header("Connection", "close");
          res.set_should_close(true);
          co_return true;
        }
      }

      const std::string target = strip_query(req.target());

      auto *node = match_handler_node(method, target);

      if (!node && is_head)
      {
        node = match_handler_node("GET", target);
      }

      if (node && node->handler)
      {
        co_await node->handler->handle_request(req, res);

        finalize_response(req, res, is_head);
        co_return true;
      }

      if (notFound_)
      {
        co_await notFound_(req, res);
        finalize_response(req, res, is_head);
      }
      else
      {
        res.set_status(vix::vhttp::NOT_FOUND);

        nlohmann::json j{
            {"error", "Route not found"},
            {"method", method},
            {"path", req.target()}};

        vix::vhttp::Response::json_response(res, j, vix::vhttp::NOT_FOUND);
        res.set_header("Connection", "close");
        res.set_should_close(true);

        finalize_response(req, res, is_head);
      }

      co_return true;
    }

    /**
     * @brief Return true if the route matched by this request is marked as heavy.
     */
    bool is_heavy(const vix::vhttp::Request &req) const
    {
      const RouteNode *node = match_node(req);
      return node ? node->heavy : false;
    }

    /**
     * @brief Remove the query string from a request target and return only the path.
     */
    static std::string strip_query(std::string target)
    {
      if (const auto q = target.find('?'); q != std::string::npos)
        target.resize(q);

      return normalize_path(target);
    }

    /**
     * @brief Return true if a handler exists for (method, path).
     */
    bool has_route(std::string method, const std::string &path) const
    {
      const std::string target = normalize_path(strip_query(path));
      const std::string full_path = normalize_method(method) + target;

      const RouteNode *node = root_.get();
      std::size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        const std::string segment = full_path.substr(start, end - start);

        if (const RouteNode *static_child = node->find_child(segment))
        {
          node = static_child;
        }
        else if (const RouteNode *param_child = node->find_child("*"))
        {
          node = param_child;
        }
        else
        {
          return false;
        }

        start = end + 1;
      }

      return node && node->has_handler();
    }

    /**
     * @brief Return the list of routes registered on this router.
     */
    const std::vector<RouteRecord> &routes() const noexcept
    {
      return registered_routes_;
    }

  private:
    std::unique_ptr<RouteNode> root_;
    NotFoundHandler notFound_{};
    std::vector<RouteRecord> registered_routes_{};

    static std::string normalize_method(std::string method)
    {
      std::transform(
          method.begin(),
          method.end(),
          method.begin(),
          [](unsigned char c)
          { return static_cast<char>(std::toupper(c)); });

      return method;
    }

    static std::string normalize_path(std::string path)
    {
      if (path.empty())
        return "/";

      if (path.front() != '/')
        path.insert(path.begin(), '/');

      while (path.size() > 1 && path.back() == '/')
        path.pop_back();

      return path;
    }

    RouteNode *match_handler_node(
        const std::string &method,
        const std::string &target)
    {
      const std::string full_path =
          normalize_method(method) + normalize_path(target);

      RouteNode *node = root_.get();
      std::size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        const std::string segment = full_path.substr(start, end - start);

        if (RouteNode *static_child = node->find_child(segment))
        {
          node = static_child;
        }
        else if (RouteNode *param_child = node->find_child("*"))
        {
          node = param_child;
        }
        else
        {
          return nullptr;
        }

        start = end + 1;
      }

      return (node && node->has_handler()) ? node : nullptr;
    }

    const RouteNode *match_node(const vix::vhttp::Request &req) const
    {
      const std::string target = strip_query(req.target());
      const std::string full_path =
          normalize_method(req.method()) + normalize_path(target);

      const RouteNode *node = root_.get();
      std::size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        const std::string segment = full_path.substr(start, end - start);

        if (const RouteNode *static_child = node->find_child(segment))
        {
          node = static_child;
        }
        else if (const RouteNode *param_child = node->find_child("*"))
        {
          node = param_child;
        }
        else
        {
          return nullptr;
        }

        start = end + 1;
      }

      return (node && node->has_handler()) ? node : nullptr;
    }

    static void finalize_response(
        const vix::vhttp::Request &req,
        vix::vhttp::Response &res,
        bool is_head)
    {
      if (res.status() == vix::vhttp::NO_CONTENT ||
          res.status() == vix::vhttp::NOT_MODIFIED)
      {
        res.set_body("");
      }

      if (is_head)
      {
        const std::size_t body_len = res.body().size();
        res.set_header("Content-Length", std::to_string(body_len));
        res.set_body("");
      }

      if (!res.has_header("Connection"))
      {
        const std::string connection = req.header("Connection");
        if (!connection.empty())
        {
          res.set_header("Connection", connection);
          res.set_should_close(connection == "close");
        }
        else
        {
          res.set_header("Connection", "keep-alive");
          res.set_should_close(false);
        }
      }

      if (!res.has_header("Content-Length"))
      {
        res.set_header("Content-Length", std::to_string(res.body().size()));
      }

      if (!res.has_header("Server"))
      {
        res.set_header("Server", "Vix.cpp");
      }

      if (!res.has_header("Date"))
      {
        res.set_header("Date", vix::vhttp::Response::http_date_now());
      }
    }
  };

} // namespace vix::router

#endif // VIX_ROUTER_HPP
