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

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/RequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/RouteDoc.hpp>
#include <vix/router/RouteNode.hpp>
#include <vix/router/RouteOptions.hpp>

namespace vix::router
{
  namespace http = boost::beast::http;

  /** @brief Lightweight route matcher/dispatcher backed by a route tree (method + path), with optional OpenAPI metadata. */
  class Router
  {
  public:
    /** @brief Custom handler called when no route matches a request. */
    using NotFoundHandler = std::function<void(
        const http::request<http::string_body> &,
        http::response<http::string_body> &)>;

    /** @brief Metadata for one registered route (used for docs/OpenAPI and runtime checks). */
    struct RouteRecord
    {
      http::verb method{};
      std::string path{};
      bool heavy{false};
      RouteDoc doc{};
    };

    /** @brief Create an empty router with an initialized route tree. */
    Router()
        : root_{std::make_unique<RouteNode>()},
          notFound_{} {}

    /** @brief Set a custom not-found handler (otherwise a default JSON 404 is returned). */
    void setNotFoundHandler(NotFoundHandler h) { notFound_ = std::move(h); }

    /** @brief Register a route handler for (method, path). */
    void add_route(
        http::verb method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler)
    {
      add_route(method, path, std::move(handler), RouteOptions{}, RouteDoc{});
    }

    /** @brief Register a route handler for (method, path) with options (e.g. heavy routes). */
    void add_route(
        http::verb method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler,
        RouteOptions opt)
    {
      add_route(method, path, std::move(handler), std::move(opt), RouteDoc{});
    }

    /** @brief Register a route handler for (method, path) with options and documentation metadata. */
    void add_route(
        http::verb method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler,
        RouteOptions opt,
        RouteDoc doc)
    {
      std::string full_path = method_to_string(method) + path;
      auto *node = root_.get();
      std::size_t start = 0;

      while (start < full_path.size())
      {
        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        std::string segment = full_path.substr(start, end - start);

        const bool isParam = !segment.empty() && segment.front() == '{' && segment.back() == '}';
        const std::string key = isParam ? "*" : segment;

        if (!node->children.count(key))
        {
          node->children[key] = std::make_unique<RouteNode>();
          node->children[key]->isParam = isParam;
          if (isParam)
            node->children[key]->paramName = segment.substr(1, segment.size() - 2);
        }

        node = node->children[key].get();
        start = end + 1;
      }

      node->handler = std::move(handler);
      node->heavy = opt.heavy;

      if (opt.heavy)
        doc.x["x-vix-heavy"] = true;

      registered_routes_.push_back(RouteRecord{
          method,
          path,
          opt.heavy,
          std::move(doc)});
    }

    /** @brief Dispatch a request to the matching handler (returns true once handled, including 404). */
    bool handle_request(
        const http::request<http::string_body> &req,
        http::response<http::string_body> &res)
    {
      const bool is_head = (req.method() == http::verb::head);

      if (req.method() == http::verb::options)
      {
        const std::string target = strip_query(std::string(req.target()));
        if (!has_route(http::verb::options, target))
        {
          res.result(http::status::no_content);
          res.set(http::field::connection, "close");
          res.content_length(0);
          res.prepare_payload();
          return true;
        }
      }

      const std::string target = strip_query(std::string(req.target()));

      auto match_handler_node = [&](http::verb m) -> RouteNode *
      {
        std::string full_path = method_to_string(m) + target;

        auto *node = root_.get();
        std::size_t start = 0;

        while (start <= full_path.size() && node)
        {
          if (start == full_path.size())
            break;

          std::size_t end = full_path.find('/', start);
          if (end == std::string::npos)
            end = full_path.size();

          std::string segment = full_path.substr(start, end - start);

          if (node->children.count(segment))
            node = node->children.at(segment).get();
          else if (node->children.count("*"))
            node = node->children.at("*").get();
          else
            return nullptr;

          start = end + 1;
        }

        if (node && node->handler)
          return node;

        return nullptr;
      };

      RouteNode *node = match_handler_node(req.method());

      if (!node && is_head)
      {
        node = match_handler_node(http::verb::get);
      }

      if (node && node->handler)
      {
        node->handler->handle_request(req, res);

        if (res.result() != http::status::unknown)
        {
          const int s = static_cast<int>(res.result());

          // 204/304 must not include a body
          if (s == 204 || s == 304)
          {
            res.body().clear();
            res.content_length(0);
            res.prepare_payload();
            return true;
          }

          if (is_head)
          {
            const std::size_t body_len = res.body().size();

            res.prepare_payload();
            res.body().clear();
            res.content_length(body_len);

            return true;
          }

          if (res.body().empty() && res.find(http::field::content_length) == res.end())
            res.content_length(0);

          res.prepare_payload();
        }

        return true;
      }

      if (notFound_)
      {
        notFound_(req, res);
        res.prepare_payload();
      }
      else
      {
        res.result(http::status::not_found);
        nlohmann::json j{
            {"error", "Route not found"},
            {"method", std::string(req.method_string())},
            {"path", std::string(req.target())}};
        vix::vhttp::Response::json_response(res, j, res.result());
        res.set(http::field::connection, "close");
        res.prepare_payload();
      }

      return true;
    }

    /** @brief Return true if the route matched by this request is marked as heavy. */
    bool is_heavy(const http::request<http::string_body> &req) const
    {
      const RouteNode *node = match_node(req);
      return node ? node->heavy : false;
    }

    /** @brief Remove the query string from a request target and return only the path. */
    static std::string strip_query(std::string target)
    {
      if (auto q = target.find('?'); q != std::string::npos)
        target.resize(q);
      return target;
    }

    /** @brief Return true if a handler exists for (method, path). */
    bool has_route(http::verb method, const std::string &path) const
    {
      std::string target = strip_query(path);
      std::string full_path = method_to_string(method) + target;

      const RouteNode *node = root_.get();
      std::size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        std::string segment = full_path.substr(start, end - start);

        if (node->children.count(segment))
          node = node->children.at(segment).get();
        else if (node->children.count("*"))
          node = node->children.at("*").get();
        else
          return false;

        start = end + 1;
      }

      return node && node->handler;
    }

    /** @brief Return the list of routes registered on this router (useful for docs generation). */
    const std::vector<RouteRecord> &routes() const noexcept { return registered_routes_; }

  private:
    std::unique_ptr<RouteNode> root_;
    NotFoundHandler notFound_{};
    std::vector<RouteRecord> registered_routes_{};

    std::string method_to_string(http::verb method) const
    {
      switch (method)
      {
      case http::verb::get:
        return "GET";
      case http::verb::post:
        return "POST";
      case http::verb::put:
        return "PUT";
      case http::verb::delete_:
        return "DELETE";
      case http::verb::patch:
        return "PATCH";
      case http::verb::head:
        return "HEAD";
      case http::verb::options:
        return "OPTIONS";
      case http::verb::trace:
        return "TRACE";
      case http::verb::connect:
        return "CONNECT";
      default:
        return "OTHER";
      }
    }

    const RouteNode *match_node(const http::request<http::string_body> &req) const
    {
      std::string target = strip_query(std::string(req.target()));
      std::string full_path = method_to_string(req.method()) + target;

      const RouteNode *node = root_.get();
      std::size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        std::size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();

        std::string segment = full_path.substr(start, end - start);

        if (node->children.count(segment))
          node = node->children.at(segment).get();
        else if (node->children.count("*"))
          node = node->children.at("*").get();
        else
          return nullptr;

        start = end + 1;
      }

      if (node && node->handler)
        return node;

      return nullptr;
    }
  };

} // namespace vix::router

#endif // VIX_ROUTER_HPP
