/**
 *
 *  @file Router.hpp
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
#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include <vix/http/Response.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/router/RouteOptions.hpp>
#include <vix/router/RouteNode.hpp>

#include <boost/beast/http.hpp>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace vix::router
{
  namespace http = boost::beast::http;

  class Router
  {
  public:
    using NotFoundHandler = std::function<void(
        const http::request<http::string_body> &,
        http::response<http::string_body> &)>;

    Router()
        : root_{std::make_unique<RouteNode>()},
          notFound_{} {}

    void setNotFoundHandler(NotFoundHandler h) { notFound_ = std::move(h); }

    void add_route(
        http::verb method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler)
    {
      add_route(method, path, std::move(handler), RouteOptions{});
    }

    void add_route(
        http::verb method,
        const std::string &path,
        std::shared_ptr<vix::vhttp::IRequestHandler> handler,
        RouteOptions opt)
    {
      std::string full_path = method_to_string(method) + path;
      auto *node = root_.get();
      size_t start = 0;

      while (start < full_path.size())
      {
        size_t end = full_path.find('/', start);
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
    }

    bool handle_request(
        const http::request<http::string_body> &req,
        http::response<http::string_body> &res)
    {
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

      std::string target = strip_query(std::string(req.target()));
      std::string full_path = method_to_string(req.method()) + target;

      auto *node = root_.get();
      size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        size_t end = full_path.find('/', start);
        if (end == std::string::npos)
          end = full_path.size();
        std::string segment = full_path.substr(start, end - start);

        if (node->children.count(segment))
        {
          node = node->children.at(segment).get();
        }
        else if (node->children.count("*"))
        {
          node = node->children.at("*").get();
        }
        else
        {
          node = nullptr;
          break;
        }

        start = end + 1;
      }

      if (node && node->handler)
      {
        node->handler->handle_request(req, res);

        if (res.result() != http::status::unknown)
        {
          const int s = static_cast<int>(res.result());
          if (s == 204 || s == 304)
          {
            res.body().clear();
          }

          if (res.body().empty() &&
              res.find(http::field::content_length) == res.end())
          {
            res.content_length(0);
          }

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

    bool is_heavy(const http::request<http::string_body> &req) const
    {
      const RouteNode *node = match_node(req);
      return node ? node->heavy : false;
    }

    static std::string strip_query(std::string target)
    {
      if (auto q = target.find('?'); q != std::string::npos)
        target.resize(q);
      return target;
    }

    bool has_route(http::verb method, const std::string &path) const
    {
      std::string target = strip_query(path);
      std::string full_path = method_to_string(method) + target;

      const RouteNode *node = root_.get();
      size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        size_t end = full_path.find('/', start);
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

  private:
    std::unique_ptr<RouteNode> root_;
    NotFoundHandler notFound_{};

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
      size_t start = 0;

      while (start <= full_path.size() && node)
      {
        if (start == full_path.size())
          break;

        size_t end = full_path.find('/', start);
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
