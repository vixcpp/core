/**
 *
 * @file OpenApi.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_OPENAPI_HPP
#define VIX_OPENAPI_HPP

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/openapi/Registry.hpp>
#include <vix/router/RouteDoc.hpp>
#include <vix/router/Router.hpp>

namespace vix::openapi
{
  namespace http = boost::beast::http;

  /** @brief Convert an HTTP verb to an OpenAPI operation key (get, post...). Returns empty if unsupported. */
  inline std::string method_to_openapi(http::verb v)
  {
    switch (v)
    {
    case http::verb::get:
      return "get";
    case http::verb::post:
      return "post";
    case http::verb::put:
      return "put";
    case http::verb::delete_:
      return "delete";
    case http::verb::patch:
      return "patch";
    case http::verb::head:
      return "head";
    default:
      return {};
    }
  }

  /** @brief Default OpenAPI responses when none are provided. */
  inline nlohmann::json default_responses()
  {
    return nlohmann::json{
        {"200", {{"description", "OK"}}}};
  }

  /** @brief Build a stable operationId from method + path. */
  inline std::string make_operation_id(std::string_view method, std::string_view path)
  {
    std::string id;
    id.reserve(method.size() + path.size() + 8);

    id.append(method);
    id.push_back('_');

    for (char c : path)
    {
      if ((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9'))
      {
        id.push_back(c);
      }
      else
      {
        id.push_back('_');
      }
    }

    while (!id.empty() && id.back() == '_')
      id.pop_back();

    return id;
  }

  /**
   * @brief Build an OpenAPI 3 document from the HTTP router and the OpenAPI registry.
   *
   * Includes core HTTP routes and extra module docs registered in Registry.
   */
  inline nlohmann::json build_from_router(
      const vix::router::Router &router,
      std::string title = "Vix API",
      std::string version = "1.31.0")
  {
    nlohmann::json doc;
    doc["openapi"] = "3.0.3";
    doc["info"] = {
        {"title", std::move(title)},
        {"version", std::move(version)}};

    doc["paths"] = nlohmann::json::object();
    doc["components"] = nlohmann::json::object();

    // Avoid duplicates if the same (method, path) appears in router + Registry.
    std::unordered_set<std::string> seen;

    auto add_doc_to_paths = [&](http::verb method,
                                const std::string &path,
                                const vix::router::RouteDoc &rdoc)
    {
      const std::string m = method_to_openapi(method);
      if (m.empty())
        return;

      const std::string key = m + " " + path;
      if (seen.find(key) != seen.end())
        return;
      seen.insert(key);

      auto &pathItem = doc["paths"][path];
      nlohmann::json op = nlohmann::json::object();

      // Stable id for client generators
      op["operationId"] = make_operation_id(m, path);

      if (!rdoc.summary.empty())
        op["summary"] = rdoc.summary;
      if (!rdoc.description.empty())
        op["description"] = rdoc.description;
      if (!rdoc.tags.empty())
        op["tags"] = rdoc.tags;

      if (!rdoc.request_body.empty())
        op["requestBody"] = rdoc.request_body;

      if (!rdoc.responses.empty())
        op["responses"] = rdoc.responses;
      else
        op["responses"] = default_responses();

      for (auto it = rdoc.x.begin(); it != rdoc.x.end(); ++it)
        op[it.key()] = it.value();

      pathItem[m] = std::move(op);
    };

    // 1) Routes declared in the HTTP router (core)
    for (const auto &r : router.routes())
      add_doc_to_paths(r.method, r.path, r.doc);

    // 2) Extra docs registered by other modules (websocket etc.)
    for (const auto &e : vix::openapi::Registry::snapshot())
      add_doc_to_paths(e.method, e.path, e.doc);

    return doc;
  }

} // namespace vix::openapi

#endif // VIX_OPENAPI_HPP
