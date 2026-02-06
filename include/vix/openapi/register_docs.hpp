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
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/RequestHandler.hpp>

#include <vix/openapi/DocsUI.hpp>
#include <vix/openapi/OpenApi.hpp>
#include <vix/openapi/assets/SwaggerAssets.hpp>

#include <vix/router/RouteDoc.hpp>
#include <vix/router/Router.hpp>

namespace vix::openapi
{
  namespace http = boost::beast::http;

  namespace detail
  {
    inline void send_bytes(vix::vhttp::ResponseWrapper &res,
                           const unsigned char *data,
                           std::size_t len)
    {
      const char *p = reinterpret_cast<const char *>(data);
      res.send(std::string_view{p, len});
    }

    template <class H>
    inline std::shared_ptr<vix::vhttp::IRequestHandler> as_handler(std::shared_ptr<H> p)
    {
      return std::static_pointer_cast<vix::vhttp::IRequestHandler>(std::move(p));
    }

    inline vix::router::RouteDoc make_docs_doc(std::string summary,
                                               std::string description,
                                               std::string code = "200",
                                               std::string out_desc = "OK")
    {
      vix::router::RouteDoc doc;
      doc.summary = std::move(summary);
      doc.description = std::move(description);
      doc.tags = {"docs"};
      doc.responses[code] = {{"description", std::move(out_desc)}};
      return doc;
    }
  } // namespace detail

  /**
   * @brief Register OpenAPI and Swagger UI routes on a router.
   *
   * Adds:
   * - GET /openapi.json                 Generated OpenAPI 3 document
   * - GET /docs                         Swagger UI page (offline, local assets)
   * - GET /docs/                        Same as /docs (no redirect, avoids loops)
   * - GET /docs/index.html              Same as /docs (optional convenience)
   * - GET /docs/swagger-ui.css          Embedded Swagger UI CSS
   * - GET /docs/swagger-ui-bundle.js    Embedded Swagger UI JS bundle
   */
  inline void register_openapi_and_docs(vix::router::Router &router,
                                        std::string title = "Vix API",
                                        std::string version = "0.0.0")
  {
    using Fn = std::function<void(vix::vhttp::Request &, vix::vhttp::ResponseWrapper &)>;

    // /openapi.json
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<Fn>>(
          "/openapi.json",
          [&router, t = std::move(title), v = std::move(version)](
              vix::vhttp::Request &,
              vix::vhttp::ResponseWrapper &res)
          {
            auto j = vix::openapi::build_from_router(router, t, v);
            res.type("application/json; charset=utf-8");
            res.header("Cache-Control", "no-store");
            res.header("X-Content-Type-Options", "nosniff");
            res.send(j);
          });

      auto doc = detail::make_docs_doc(
          "OpenAPI spec",
          "Generated OpenAPI 3.0 document for this Vix HTTP router.",
          "200",
          "OpenAPI JSON");

      router.add_route(http::verb::get,
                       "/openapi.json",
                       detail::as_handler(h),
                       vix::router::RouteOptions{},
                       std::move(doc));
    }

    // /docs/swagger-ui.css
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<Fn>>(
          "/docs/swagger-ui.css",
          [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
          {
            res.type("text/css; charset=utf-8");
            res.header("Cache-Control", "public, max-age=31536000, immutable");
            res.header("X-Content-Type-Options", "nosniff");
            detail::send_bytes(res,
                               vix::openapi::assets::swagger_ui_css,
                               vix::openapi::assets::swagger_ui_css_len);
          });

      auto doc = detail::make_docs_doc(
          "Swagger UI CSS (offline)",
          "Embedded Swagger UI stylesheet served locally.",
          "200",
          "CSS");

      router.add_route(http::verb::get,
                       "/docs/swagger-ui.css",
                       detail::as_handler(h),
                       vix::router::RouteOptions{},
                       std::move(doc));
    }

    // /docs/swagger-ui-bundle.js
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<Fn>>(
          "/docs/swagger-ui-bundle.js",
          [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
          {
            res.type("application/javascript; charset=utf-8");
            res.header("Cache-Control", "public, max-age=31536000, immutable");
            res.header("X-Content-Type-Options", "nosniff");
            detail::send_bytes(res,
                               vix::openapi::assets::swagger_ui_bundle_js,
                               vix::openapi::assets::swagger_ui_bundle_js_len);
          });

      auto doc = detail::make_docs_doc(
          "Swagger UI bundle JS (offline)",
          "Embedded Swagger UI JS bundle served locally.",
          "200",
          "JavaScript");

      router.add_route(http::verb::get,
                       "/docs/swagger-ui-bundle.js",
                       detail::as_handler(h),
                       vix::router::RouteOptions{},
                       std::move(doc));
    }

    // HTML handler reused for /docs, /docs/, /docs/index.html
    auto serve_docs_html = [](vix::vhttp::Request &, vix::vhttp::ResponseWrapper &res)
    {
      res.type("text/html; charset=utf-8");
      res.header("Cache-Control", "no-store");
      res.header("X-Content-Type-Options", "nosniff");
      res.send(vix::openapi::swagger_ui_html("/openapi.json"));
    };

    // /docs
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<Fn>>("/docs", serve_docs_html);

      auto doc = detail::make_docs_doc(
          "Interactive API docs",
          "Swagger UI page that renders /openapi.json (offline assets).",
          "200",
          "HTML page");

      router.add_route(http::verb::get,
                       "/docs",
                       detail::as_handler(h),
                       vix::router::RouteOptions{},
                       std::move(doc));
    }

    // /docs/  (no redirect)
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<Fn>>("/docs/", serve_docs_html);

      auto doc = detail::make_docs_doc(
          "Interactive API docs (slash)",
          "Same as /docs but accepts trailing slash to avoid redirect loops.",
          "200",
          "HTML page");

      router.add_route(http::verb::get,
                       "/docs/",
                       detail::as_handler(h),
                       vix::router::RouteOptions{},
                       std::move(doc));
    }

    // /docs/index.html (optional convenience)
    {
      auto h = std::make_shared<vix::vhttp::RequestHandler<Fn>>("/docs/index.html", serve_docs_html);

      auto doc = detail::make_docs_doc(
          "Interactive API docs (index)",
          "Same as /docs. Convenience path for static-like expectations.",
          "200",
          "HTML page");

      router.add_route(http::verb::get,
                       "/docs/index.html",
                       detail::as_handler(h),
                       vix::router::RouteOptions{},
                       std::move(doc));
    }
  }

} // namespace vix::openapi

#endif // VIX_REGISTER_DOCS_HPP
