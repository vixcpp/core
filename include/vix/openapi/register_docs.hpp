/**
 *
 * @file register_docs.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_REGISTER_DOCS_HPP
#define VIX_REGISTER_DOCS_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/router/Router.hpp>
#include <vix/router/RouteDoc.hpp>

#include <vix/openapi/OpenApi.hpp>
#include <vix/openapi/DocsUI.hpp>
#include <vix/openapi/assets/SwaggerAssets.hpp>

#include <vix/http/RequestHandler.hpp>
#include <vix/http/IRequestHandler.hpp>

namespace vix::openapi
{
  namespace http = boost::beast::http;

  namespace detail
  {
    inline void send_bytes(
        vix::vhttp::ResponseWrapper &res,
        const unsigned char *data,
        std::size_t len)
    {
      const char *p = reinterpret_cast<const char *>(data);
      res.send(std::string_view{p, len});
    }
  }

  /**
   * @brief Register OpenAPI and Swagger UI routes on a router.
   *
   * Adds:
   * - GET /openapi.json             Generated OpenAPI 3 document
   * - GET /docs                    Swagger UI page (offline, local assets)
   * - GET /docs/swagger-ui.css      Embedded Swagger UI CSS
   * - GET /docs/swagger-ui-bundle.js Embedded Swagger UI JS bundle
   */
  inline void register_openapi_and_docs(
      vix::router::Router &router,
      std::string title = "Vix API",
      std::string version = "0.0.0")
  {
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

    // /docs/swagger-ui.css (offline asset)
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<std::function<void(
          vix::vhttp::Request &, vix::vhttp::ResponseWrapper &)>>>(
          "/docs/swagger-ui.css",
          [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
          {
            res.type("text/css; charset=utf-8");
            res.header("Cache-Control", "public, max-age=86400");
            res.header("X-Content-Type-Options", "nosniff");

            vix::openapi::detail::send_bytes(
                res,
                vix::openapi::assets::swagger_ui_css,
                vix::openapi::assets::swagger_ui_css_len);
          });

      vix::router::RouteDoc doc;
      doc.summary = "Swagger UI CSS (offline)";
      doc.description = "Embedded Swagger UI stylesheet served locally.";
      doc.tags = {"docs"};
      doc.responses["200"] = {{"description", "CSS"}};

      router.add_route(
          http::verb::get,
          "/docs/swagger-ui.css",
          as_handler(h),
          vix::router::RouteOptions{},
          std::move(doc));
    }

    // /docs/swagger-ui-bundle.js (offline asset)
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<std::function<void(
          vix::vhttp::Request &, vix::vhttp::ResponseWrapper &)>>>(
          "/docs/swagger-ui-bundle.js",
          [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
          {
            res.type("application/javascript; charset=utf-8");
            res.header("Cache-Control", "public, max-age=86400");
            res.header("X-Content-Type-Options", "nosniff");

            vix::openapi::detail::send_bytes(
                res,
                vix::openapi::assets::swagger_ui_bundle_js,
                vix::openapi::assets::swagger_ui_bundle_js_len);
          });

      vix::router::RouteDoc doc;
      doc.summary = "Swagger UI bundle JS (offline)";
      doc.description = "Embedded Swagger UI JS bundle served locally.";
      doc.tags = {"docs"};
      doc.responses["200"] = {{"description", "JavaScript"}};

      router.add_route(
          http::verb::get,
          "/docs/swagger-ui-bundle.js",
          as_handler(h),
          vix::router::RouteOptions{},
          std::move(doc));
    }

    // /docs (HTML that references local assets)
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<std::function<void(
          vix::vhttp::Request &, vix::vhttp::ResponseWrapper &)>>>(
          "/docs",
          [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
          {
            res.type("text/html; charset=utf-8");
            res.header("Cache-Control", "no-store");
            res.header("X-Content-Type-Options", "nosniff");
            res.send(vix::openapi::swagger_ui_html("/openapi.json"));
          });

      vix::router::RouteDoc doc;
      doc.summary = "Interactive API docs";
      doc.description = "Swagger UI page that renders /openapi.json (offline assets).";
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
