/**
 * @file DocsUI.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_DOCS_UI_HPP
#define VIX_DOCS_UI_HPP

#include <string>
#include <utility>

namespace vix::openapi
{
  /**
   * @brief Return a Swagger UI HTML page that uses local (offline) assets.
   *
   * The page expects these routes to exist:
   * - /docs/swagger-ui.css
   * - /docs/swagger-ui-bundle.js
   *
   * And it loads the OpenAPI document from @p openapi_url (default: /openapi.json).
   */
  inline std::string swagger_ui_html(std::string openapi_url = "/openapi.json")
  {
    std::string html;
    html.reserve(4096);

    html += "<!doctype html>";
    html += "<html><head><meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Vix Docs</title>";

    // Offline assets served by Vix routes (no external CDN)
    html += "<link rel=\"stylesheet\" href=\"/docs/swagger-ui.css\">";

    html += "</head><body>";
    html += "<div id=\"swagger-ui\"></div>";

    // Offline JS bundle served by Vix routes
    html += "<script src=\"/docs/swagger-ui-bundle.js\"></script>";
    html += "<script>";
    html += "window.onload=function(){";
    html += "SwaggerUIBundle({";
    html += "url:'" + std::move(openapi_url) + "',";
    html += "dom_id:'#swagger-ui'";
    html += "});";
    html += "};";
    html += "</script>";

    html += "</body></html>";
    return html;
  }

} // namespace vix::openapi

#endif // VIX_DOCS_UI_HPP
