/**
 *
 * @file register_docs.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_REGISTER_DOCS_HPP
#define VIX_REGISTER_DOCS_HPP

#include <memory>
#include <string>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/router/Router.hpp>
#include <vix/router/RouteDoc.hpp>

#include <vix/openapi/OpenApi.hpp>
#include <vix/openapi/DocsUI.hpp>

#include <vix/http/RequestHandler.hpp>
#include <vix/http/IRequestHandler.hpp>

namespace vix::openapi
{

  /**
   * @brief Register OpenAPI and Swagger UI routes on a router.
   *
   * Adds:
   * - GET /openapi.json : generated OpenAPI 3 document
   * - GET /docs        : Swagger UI page rendering /openapi.json
   */
  inline void register_openapi_and_docs(
      vix::router::Router &router,
      std::string title = "Vix API",
      std::string version = "0.0.0")
  {
    namespace http = boost::beast::http;

    auto as_handler = [](auto p) -> std::shared_ptr<vix::vhttp::IRequestHandler>
    {
      return std::static_pointer_cast<vix::vhttp::IRequestHandler>(std::move(p));
    };

    // /openapi.json
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<std::function<void(
          vix::vhttp::Request &, vix::vhttp::ResponseWrapper &)>>>(
          "/openapi.json",
          [&, t = std::move(title), v = std::move(version)](
              vix::vhttp::Request &,
              vix::vhttp::ResponseWrapper &res) mutable
          {
            auto j = vix::openapi::build_from_router(router, t, v);
            res.type("application/json; charset=utf-8");
            res.header("Cache-Control", "no-store");
            res.send(j);
          });

      vix::router::RouteDoc doc;
      doc.summary = "OpenAPI spec";
      doc.description = "Generated OpenAPI 3.0 document for this Vix HTTP router.";
      doc.tags = {"docs"};
      doc.responses["200"] = {{"description", "OpenAPI JSON"}};

      router.add_route(
          http::verb::get,
          "/openapi.json",
          as_handler(h),
          vix::router::RouteOptions{},
          std::move(doc));
    }

    // /docs
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<std::function<void(
          vix::vhttp::Request &, vix::vhttp::ResponseWrapper &)>>>(
          "/docs",
          [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
          {
            res.type("text/html; charset=utf-8");
            res.header("Cache-Control", "no-store");
            res.send(vix::openapi::swagger_ui_html("/openapi.json"));
          });

      vix::router::RouteDoc doc;
      doc.summary = "Interactive API docs";
      doc.description = "Swagger UI page that renders /openapi.json.";
      doc.tags = {"docs"};
      doc.responses["200"] = {{"description", "HTML page"}};

      router.add_route(
          http::verb::get,
          "/docs",
          as_handler(h),
          vix::router::RouteOptions{},
          std::move(doc));
    }
  }

} // namespace vix::openapi

#endif // VIX_REGISTER_DOCS_HPP
