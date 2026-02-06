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
#include <string_view>
#include <utility>

namespace vix::openapi
{
  namespace detail
  {
    inline void append_js_escaped(std::string &out, std::string_view s)
    {
      for (char c : s)
      {
        switch (c)
        {
        case '\\':
          out += "\\\\";
          break;
        case '\'':
          out += "\\\'";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          out.push_back(c);
          break;
        }
      }
    }

    inline std::string normalize_openapi_url(std::string url)
    {
      if (url.empty())
        return "/openapi.json";
      return url;
    }
  } // namespace detail

  /**
   * @brief Swagger UI HTML using local offline assets.
   *
   * Routes expected:
   * - /docs/swagger-ui.css
   * - /docs/swagger-ui-bundle.js
   *
   * Important:
   * - Works for BOTH /docs and /docs/ thanks to <base href="/docs/">
   */
  inline std::string swagger_ui_html(std::string openapi_url = "/openapi.json")
  {
    openapi_url = detail::normalize_openapi_url(std::move(openapi_url));

    std::string html;
    html.reserve(8000);

    html += "<!doctype html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Vix Docs</title>";

    // KEY FIX: force a stable base so relative assets always resolve to /docs/*
    html += "<base href=\"/docs/\">";

    // Now these always become: /docs/swagger-ui.css and /docs/swagger-ui-bundle.js
    html += "<link rel=\"stylesheet\" href=\"swagger-ui.css\">";

    // Small hardening
    html += "<style>";
    html += "html,body{height:100%;margin:0;padding:0}";
    html += "#swagger-ui{min-height:100%}";
    html += ".swagger-ui .topbar{display:block}";
    html += "</style>";

    html += "</head>";
    html += "<body>";
    html += "<div id=\"swagger-ui\"></div>";

    html += "<script src=\"swagger-ui-bundle.js\"></script>";
    html += "<script>";
    html += "(function(){";
    html += "function mount(){";
    html += "var el=document.getElementById('swagger-ui');";
    html += "if(!el) return;";
    html += "el.innerHTML='';";
    html += "if(!window.SwaggerUIBundle){ console.error('SwaggerUIBundle missing'); return; }";
    html += "try{";
    html += "window.ui=SwaggerUIBundle({";
    html += "url:'";
    detail::append_js_escaped(html, openapi_url);
    html += "',";
    html += "dom_id:'#swagger-ui',";
    html += "deepLinking:true,";
    html += "persistAuthorization:true,";
    html += "displayRequestDuration:true";
    html += "});";
    html += "}catch(e){ console.error('SwaggerUI init failed', e); }";
    html += "}";
    html += "if(document.readyState==='loading'){";
    html += "document.addEventListener('DOMContentLoaded', mount);";
    html += "}else{ mount(); }";
    html += "window.addEventListener('pageshow', mount);";
    html += "})();";
    html += "</script>";

    html += "</body>";
    html += "</html>";
    return html;
  }

} // namespace vix::openapi

#endif // VIX_DOCS_UI_HPP
