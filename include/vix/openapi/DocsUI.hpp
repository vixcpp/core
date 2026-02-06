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
    html.reserve(18000);

    html += "<!doctype html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Vix Docs</title>";

    // Stable base so relative assets resolve to /docs/*
    html += "<base href=\"/docs/\">";

    // Offline swagger assets served by Vix
    html += "<link rel=\"stylesheet\" href=\"swagger-ui.css\">";

    html += "<style>";

    // vixcpp.com theme tokens
    html += ":root{";
    html += "--bg:#011e1c;";
    html += "--bg-alt:#022724;";
    html += "--bg-elevated:#03312d;";
    html += "--accent:#1ee6a3;";
    html += "--accent-dark:#0ca377;";
    html += "--accent-soft:rgba(30,230,163,0.16);";
    html += "--text:#ffffff;";
    html += "--muted:#cbd5e1;";
    html += "--border:#09433f;";
    html += "--radius-lg:20px;";
    html += "--radius-md:14px;";
    html += "--shadow-soft:0 22px 45px rgba(0,0,0,0.6);";
    html += "--sans:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial;";
    html += "--mono:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono','Courier New',monospace;";
    html += "}";

    // Page base
    html += "html,body{height:100%;margin:0;padding:0;background:var(--bg);color:var(--text)}";
    html += "body{font-family:var(--sans)}";
    html += "a{color:var(--accent)}";

    // Vix header
    html += ".vix-top{position:sticky;top:0;z-index:100;";
    html += "background:linear-gradient(180deg,var(--bg-elevated),var(--bg));";
    html += "border-bottom:1px solid var(--border)}";
    html += ".vix-top-inner{max-width:1120px;margin:0 auto;padding:14px 16px;";
    html += "display:flex;align-items:center;justify-content:space-between;gap:12px}";
    html += ".vix-brand{display:flex;align-items:center;gap:10px;min-width:0}";
    html += ".vix-dot{width:10px;height:10px;border-radius:999px;background:var(--accent);";
    html += "box-shadow:0 0 0 6px var(--accent-soft)}";
    html += ".vix-title{font-weight:800;letter-spacing:.2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}";
    html += ".vix-sub{color:var(--muted);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}";
    html += ".vix-actions{display:flex;align-items:center;gap:10px;flex-wrap:wrap;justify-content:flex-end}";
    html += ".vix-pill{display:inline-flex;align-items:center;gap:8px;";
    html += "padding:8px 10px;border-radius:999px;border:1px solid var(--border);";
    html += "background:rgba(3,49,45,.55);color:var(--muted);font-size:12px}";
    html += ".vix-pill b{color:var(--text);font-weight:700}";
    html += ".vix-pill code{font-family:var(--mono);font-size:12px;color:var(--text)}";
    html += ".vix-pill .vix-loading{opacity:.85}";
    html += ".vix-link{display:inline-flex;align-items:center;gap:6px;text-decoration:none}";
    html += ".vix-link:hover{filter:brightness(1.05)}";

    // Swagger layout wrapper
    html += ".swagger-ui .wrapper{max-width:1120px;margin:0 auto;padding:16px}";
    html += "#swagger-ui{min-height:100%}";

    // Hide Swagger topbar and Swagger generated info header (double header)
    html += ".swagger-ui .topbar{display:none !important}";
    html += ".swagger-ui .information-container{display:none !important}";
    html += ".swagger-ui .info{display:none !important}";
    html += ".swagger-ui .wrapper{padding-top:10px}";

    // Panels / cards
    html += ".swagger-ui .scheme-container{";
    html += "background:rgba(3,49,45,.55);border:1px solid var(--border);";
    html += "border-radius:var(--radius-lg);box-shadow:var(--shadow-soft)}";

    html += ".swagger-ui .opblock{";
    html += "border:1px solid var(--border);border-radius:var(--radius-lg);";
    html += "box-shadow:var(--shadow-soft);overflow:hidden}";
    html += ".swagger-ui .opblock .opblock-summary{border-bottom:1px solid var(--border)}";
    html += ".swagger-ui .opblock .opblock-summary-description{color:var(--muted)}";

    // Buttons
    html += ".swagger-ui .btn{border-radius:12px;border:1px solid var(--border);box-shadow:none}";
    html += ".swagger-ui .btn.execute,.swagger-ui .btn.authorize{";
    html += "background:var(--accent);border-color:var(--accent);color:#001412;font-weight:800}";
    html += ".swagger-ui .btn.execute:hover,.swagger-ui .btn.authorize:hover{filter:brightness(1.03)}";

    // Inputs
    html += ".swagger-ui input[type=text],.swagger-ui input[type=password],.swagger-ui textarea{";
    html += "background:rgba(3,49,45,.55);border:1px solid var(--border);";
    html += "border-radius:12px;color:var(--text)}";
    html += ".swagger-ui label{color:var(--muted)}";

    // Code blocks
    html += ".swagger-ui pre,.swagger-ui code{font-family:var(--mono)}";
    html += ".swagger-ui .highlight-code{background:rgba(3,49,45,.55);";
    html += "border:1px solid var(--border);border-radius:var(--radius-md)}";
    html += ".swagger-ui .microlight{color:var(--text)}";

    // Tables
    html += ".swagger-ui table thead tr th{color:var(--muted);border-bottom:1px solid var(--border)}";
    html += ".swagger-ui table tbody tr td{color:var(--text);border-bottom:1px solid var(--border)}";

    // Fix low contrast defaults (#3b4151 etc.)
    html += ".swagger-ui, .swagger-ui *{color:var(--text)}";
    html += ".swagger-ui .opblock-summary-path,"
            ".swagger-ui .opblock-summary-description,"
            ".swagger-ui .parameter__name,"
            ".swagger-ui .parameter__type,"
            ".swagger-ui .response-col_status,"
            ".swagger-ui .responses-inner h4,"
            ".swagger-ui .responses-inner h5,"
            ".swagger-ui .model-title{color:var(--text) !important}";
    html += ".swagger-ui .markdown p,"
            ".swagger-ui .markdown li,"
            ".swagger-ui .tab li,"
            ".swagger-ui .opblock-description-wrapper p,"
            ".swagger-ui .opblock-external-docs-wrapper p,"
            ".swagger-ui .opblock-title_normal{color:var(--muted) !important}";

    html += ".swagger-ui a,.swagger-ui a:visited{color:var(--accent) !important}";
    html += ".swagger-ui a:hover{color:var(--accent) !important;filter:brightness(1.05)}";

    // Control arrow (expand/collapse)
    html += ".swagger-ui .opblock-summary-control svg{fill:var(--muted) !important}";
    html += ".swagger-ui .opblock-summary-control svg:hover{fill:var(--text) !important}";
    html += ".swagger-ui .arrow{fill:var(--muted) !important}";
    html += ".swagger-ui .opblock-title span{";
    html += "color:var(--muted) !important;";
    html += "}";

    html += "</style>";

    html += "</head>";
    html += "<body>";

    // Vix header (dynamic title/version from OpenAPI JSON)
    html += "<header class=\"vix-top\">";
    html += "<div class=\"vix-top-inner\">";
    html += "<div class=\"vix-brand\">";
    html += "<span class=\"vix-dot\"></span>";
    html += "<div style=\"min-width:0\">";
    html += "<div class=\"vix-title\" id=\"vix-docs-title\">Vix API</div>";
    html += "<div class=\"vix-sub\" id=\"vix-docs-sub\"><span class=\"vix-loading\">Loading OpenAPI...</span></div>";
    html += "</div>";
    html += "</div>";
    html += "<div class=\"vix-actions\">";
    html += "<span class=\"vix-pill\"><b>Spec</b>"
            "<a class=\"vix-link\" id=\"vix-openapi-link\" href=\"/openapi.json\" target=\"_blank\" rel=\"noopener\">"
            "<code id=\"vix-openapi-path\">/openapi.json</code>"
            "</a></span>";
    html += "<span class=\"vix-pill\"><b>Version</b><span id=\"vix-openapi-version\">-</span></span>";
    html += "</div>";
    html += "</div>";
    html += "</header>";

    html += "<div id=\"swagger-ui\"></div>";

    html += "<script src=\"swagger-ui-bundle.js\"></script>";
    html += "<script>";
    html += "(function(){";

    html += "var OPENAPI_URL='";
    detail::append_js_escaped(html, openapi_url);
    html += "';";

    html += "function setText(id, value){";
    html += "var el=document.getElementById(id);";
    html += "if(el) el.textContent=value;";
    html += "}";

    html += "function loadInfo(){";
    html += "setText('vix-openapi-path', OPENAPI_URL);";
    html += "var a=document.getElementById('vix-openapi-link');";
    html += "if(a) a.setAttribute('href', OPENAPI_URL);";

    html += "fetch(OPENAPI_URL, {cache:'no-store'})";
    html += ".then(function(r){ if(!r.ok) throw new Error('openapi fetch failed: '+r.status); return r.json(); })";
    html += ".then(function(j){";
    html += "var title=(j&&j.info&&j.info.title)?String(j.info.title):'Vix API';";
    html += "var version=(j&&j.info&&j.info.version)?String(j.info.version):'-';";
    html += "setText('vix-docs-title', title);";
    html += "setText('vix-openapi-version', version);";
    html += "setText('vix-docs-sub', 'OpenAPI 3.0.3');";
    html += "})";
    html += ".catch(function(e){";
    html += "setText('vix-docs-sub', 'OpenAPI not available');";
    html += "console.error(e);";
    html += "});";
    html += "}";

    html += "function mount(){";
    html += "var el=document.getElementById('swagger-ui');";
    html += "if(!el) return;";
    html += "el.innerHTML='';";
    html += "loadInfo();";
    html += "if(!window.SwaggerUIBundle){ console.error('SwaggerUIBundle missing'); return; }";
    html += "try{";
    html += "window.ui=SwaggerUIBundle({";
    html += "url:OPENAPI_URL,";
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
