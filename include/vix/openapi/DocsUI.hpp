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
   * Design: matches vixcpp.com / registry.vixcpp.com
   * - Background:  #0e0e10
   * - Accent:      #22c55e
   * - Font:        system-ui + JetBrains Mono
   * - Topbar:      same SVG logo as RegistryBrowse
   *
   * Works for BOTH /docs and /docs/ thanks to <base href="/docs/">
   */
  inline std::string swagger_ui_html(std::string openapi_url = "/openapi.json")
  {
    openapi_url = detail::normalize_openapi_url(std::move(openapi_url));

    std::string h;
    h.reserve(22000);

    // ── HTML head ──
    h += "<!doctype html>"
         "<html lang=\"en\">"
         "<head>"
         "<meta charset=\"utf-8\">"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         "<title>Vix API Docs</title>"
         "<base href=\"/docs/\">"
         "<link rel=\"stylesheet\" href=\"swagger-ui.css\">";

    // ── CSS ──
    h += "<style>";

    // Tokens
    h += ":root{"
         "--bg:#0e0e10;"
         "--bg-soft:#161618;"
         "--bg-card:rgba(255,255,255,.03);"
         "--border:rgba(255,255,255,.08);"
         "--divider:rgba(255,255,255,.06);"
         "--accent:#22c55e;"
         "--accent-s:rgba(34,197,94,.12);"
         "--accent-b:rgba(34,197,94,.25);"
         "--text:rgba(240,240,242,.92);"
         "--muted:rgba(240,240,242,.55);"
         "--muted2:rgba(240,240,242,.28);"
         "--mono:'JetBrains Mono',ui-monospace,SFMono-Regular,Menlo,monospace;"
         "--sans:system-ui,-apple-system,sans-serif;"
         "}";

    // Page
    h += "html,body{height:100%;margin:0;padding:0;background:var(--bg);color:var(--text);"
         "font-family:var(--sans);-webkit-font-smoothing:antialiased}"
         "a{color:var(--accent);text-decoration:none}"
         "a:hover{filter:brightness(1.08)}";

    // ── Vix topbar ──
    h += ".vix-topbar{"
         "position:sticky;top:0;z-index:100;"
         "background:rgba(14,14,16,.9);"
         "backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);"
         "border-bottom:1px solid var(--divider)"
         "}";

    h += ".vix-topbar-inner{"
         "max-width:1280px;margin:0 auto;padding:0 1.5rem;"
         "height:52px;"
         "display:grid;grid-template-columns:180px 1fr auto;"
         "gap:16px;align-items:center"
         "}";

    // Brand (logo + text)
    h += ".vix-brand{display:inline-flex;align-items:center;gap:7px;text-decoration:none}"
         ".vix-brand-mark{width:22px;height:22px}"
         ".vix-brand-name{font-size:.95rem;font-weight:800;color:var(--accent);letter-spacing:-.3px}"
         ".vix-brand-dim{font-size:.88rem;font-weight:400;color:var(--muted2)}";

    // Topbar pills / meta
    h += ".vix-meta{display:flex;align-items:center;gap:12px;min-width:0}";

    h += ".vix-pill{"
         "display:inline-flex;align-items:center;gap:7px;"
         "padding:5px 11px;border-radius:999px;"
         "border:1px solid var(--border);"
         "background:var(--bg-card);"
         "font-size:.75rem;font-weight:600;color:var(--muted);"
         "white-space:nowrap;overflow:hidden;text-overflow:ellipsis"
         "}";

    h += ".vix-pill b{color:var(--text);font-weight:700}"
         ".vix-pill code{font-family:var(--mono);font-size:.72rem;color:var(--text)}";

    // Topbar right
    h += ".vix-topbar-right{display:flex;align-items:center;gap:10px;flex-shrink:0}";

    h += ".vix-spec-link{"
         "display:inline-flex;align-items:center;gap:5px;"
         "padding:5px 12px;border-radius:7px;"
         "font-size:.78rem;font-weight:600;"
         "font-family:var(--mono);"
         "background:var(--accent-s);color:var(--accent);"
         "border:1px solid var(--accent-b);"
         "text-decoration:none"
         "}"
         ".vix-spec-link:hover{background:rgba(34,197,94,.18)}";

    h += ".vix-ver-pill{"
         "display:inline-flex;align-items:center;"
         "padding:5px 10px;border-radius:999px;"
         "font-size:.72rem;font-weight:700;"
         "border:1px solid var(--border);"
         "background:var(--bg-card);color:var(--muted)"
         "}";

    // ── Swagger overrides ──
    h += ".swagger-ui .wrapper{max-width:1280px;margin:0 auto;padding:20px 1.5rem}"
         "#swagger-ui{min-height:100%}";

    // Hide Swagger's own header + info (we have our own)
    h += ".swagger-ui .topbar{display:none !important}"
         ".swagger-ui .information-container{display:none !important}"
         ".swagger-ui .info{display:none !important}"
         ".swagger-ui .wrapper{padding-top:12px}";

    // Global text fix — Swagger defaults are low-contrast
    h += ".swagger-ui,.swagger-ui *{color:var(--text)}"
         ".swagger-ui .markdown p,"
         ".swagger-ui .markdown li,"
         ".swagger-ui .opblock-description-wrapper p,"
         ".swagger-ui .opblock-external-docs-wrapper p,"
         ".swagger-ui .opblock-title_normal,"
         ".swagger-ui .tab li{color:var(--muted) !important}"
         ".swagger-ui a,.swagger-ui a:visited{color:var(--accent) !important}";

    // Scheme container
    h += ".swagger-ui .scheme-container{"
         "background:var(--bg-card);"
         "border:1px solid var(--border);"
         "border-radius:10px;"
         "box-shadow:none"
         "}";

    // Operation blocks
    h += ".swagger-ui .opblock{"
         "border:1px solid var(--border);"
         "border-radius:10px;"
         "box-shadow:0 2px 8px rgba(0,0,0,.25);"
         "overflow:hidden;margin-bottom:12px"
         "}"
         ".swagger-ui .opblock .opblock-summary{"
         "border-bottom:1px solid var(--divider);"
         "padding:10px 14px"
         "}"
         ".swagger-ui .opblock .opblock-summary-description{color:var(--muted)}";

    // Method badges — match Vix green palette
    h += ".swagger-ui .opblock.opblock-get .opblock-summary-method{"
         "background:#22c55e;color:#052e16;font-weight:700;border-radius:6px"
         "}"
         ".swagger-ui .opblock.opblock-post .opblock-summary-method{"
         "background:#3b82f6;color:#fff;font-weight:700;border-radius:6px"
         "}"
         ".swagger-ui .opblock.opblock-put .opblock-summary-method{"
         "background:#f59e0b;color:#1c1917;font-weight:700;border-radius:6px"
         "}"
         ".swagger-ui .opblock.opblock-delete .opblock-summary-method{"
         "background:#ef4444;color:#fff;font-weight:700;border-radius:6px"
         "}"
         ".swagger-ui .opblock.opblock-patch .opblock-summary-method{"
         "background:#8b5cf6;color:#fff;font-weight:700;border-radius:6px"
         "}";

    // Expand/collapse backgrounds
    h += ".swagger-ui .opblock.opblock-get{background:rgba(34,197,94,.04);border-color:rgba(34,197,94,.18)}"
         ".swagger-ui .opblock.opblock-post{background:rgba(59,130,246,.04);border-color:rgba(59,130,246,.18)}"
         ".swagger-ui .opblock.opblock-put{background:rgba(245,158,11,.04);border-color:rgba(245,158,11,.18)}"
         ".swagger-ui .opblock.opblock-delete{background:rgba(239,68,68,.04);border-color:rgba(239,68,68,.18)}"
         ".swagger-ui .opblock.opblock-patch{background:rgba(139,92,246,.04);border-color:rgba(139,92,246,.18)}";

    // Path text
    h += ".swagger-ui .opblock-summary-path{"
         "color:var(--text) !important;"
         "font-family:var(--mono);font-size:.82rem;font-weight:600"
         "}";

    // Buttons
    h += ".swagger-ui .btn{"
         "border-radius:7px;border:1px solid var(--border);box-shadow:none;"
         "font-weight:600;font-size:.82rem"
         "}"
         ".swagger-ui .btn.execute,.swagger-ui .btn.authorize{"
         "background:var(--accent);border-color:var(--accent);color:#052e16;font-weight:700"
         "}"
         ".swagger-ui .btn.execute:hover,.swagger-ui .btn.authorize:hover{"
         "background:#4ade80"
         "}";

    // Inputs
    h += ".swagger-ui input[type=text],"
         ".swagger-ui input[type=password],"
         ".swagger-ui textarea{"
         "background:var(--bg-soft);"
         "border:1px solid var(--border);"
         "border-radius:8px;"
         "color:var(--text);"
         "font-family:var(--mono);font-size:.82rem;"
         "padding:8px 10px"
         "}"
         ".swagger-ui input:focus,.swagger-ui textarea:focus{"
         "border-color:var(--accent);outline:none;"
         "box-shadow:0 0 0 3px var(--accent-s)"
         "}"
         ".swagger-ui label{color:var(--muted)}";

    // Code blocks
    h += ".swagger-ui pre,.swagger-ui code{font-family:var(--mono);font-size:.82rem}"
         ".swagger-ui .highlight-code{"
         "background:var(--bg-soft);"
         "border:1px solid var(--border);border-radius:8px"
         "}"
         ".swagger-ui .microlight{color:var(--text)}";

    // Tables
    h += ".swagger-ui table thead tr th{"
         "color:var(--muted);border-bottom:1px solid var(--border);"
         "font-size:.75rem;font-weight:700;letter-spacing:.04em;text-transform:uppercase"
         "}"
         ".swagger-ui table tbody tr td{"
         "color:var(--text);border-bottom:1px solid var(--divider)"
         "}";

    // Parameters
    h += ".swagger-ui .opblock-summary-path,"
         ".swagger-ui .opblock-summary-description,"
         ".swagger-ui .parameter__name,"
         ".swagger-ui .parameter__type,"
         ".swagger-ui .response-col_status,"
         ".swagger-ui .responses-inner h4,"
         ".swagger-ui .responses-inner h5,"
         ".swagger-ui .model-title{color:var(--text) !important}";

    // Models / schemas
    h += ".swagger-ui .model-box{"
         "background:var(--bg-soft);"
         "border:1px solid var(--border);border-radius:8px;"
         "padding:12px"
         "}";

    // Tags section
    h += ".swagger-ui .opblock-tag{"
         "border-bottom:1px solid var(--divider);"
         "font-weight:700;font-size:.9rem;letter-spacing:-.01em"
         "}"
         ".swagger-ui .opblock-tag:hover{background:var(--bg-card)}";

    // Arrows / expand controls
    h += ".swagger-ui .opblock-summary-control svg{fill:var(--muted) !important}"
         ".swagger-ui .opblock-summary-control svg:hover{fill:var(--text) !important}"
         ".swagger-ui .arrow{fill:var(--muted) !important}"
         ".swagger-ui .expand-operation svg{fill:var(--muted) !important}";

    // Try-it-out area
    h += ".swagger-ui .try-out__btn{"
         "border-radius:7px;border:1px solid var(--border);"
         "background:var(--bg-card);color:var(--text);"
         "font-size:.78rem;font-weight:600"
         "}"
         ".swagger-ui .try-out__btn:hover{"
         "border-color:var(--accent-b);background:var(--accent-s)"
         "}";

    // Response body
    h += ".swagger-ui .responses-wrapper .response-col_description{"
         "color:var(--muted) !important"
         "}";

    // Scrollbars
    h += ".swagger-ui pre::-webkit-scrollbar{height:6px;width:6px}"
         ".swagger-ui pre::-webkit-scrollbar-thumb{background:rgba(34,197,94,.25);border-radius:999px}"
         ".swagger-ui pre::-webkit-scrollbar-track{background:transparent}";

    // Responsive
    h += "@media(max-width:640px){"
         ".vix-topbar-inner{grid-template-columns:auto 1fr;height:auto;padding:8px 1rem;gap:8px}"
         ".vix-meta{grid-column:1/-1;overflow-x:auto}"
         ".vix-topbar-right{display:none}"
         ".swagger-ui .wrapper{padding:12px 1rem}"
         "}";

    h += "</style>";

    h += "</head>";
    h += "<body>";

    // ── Topbar HTML ──
    h += "<header class=\"vix-topbar\">"
         "<div class=\"vix-topbar-inner\">";

    // Brand with SVG logo (same as RegistryBrowse)
    h += "<a class=\"vix-brand\" href=\"/\" aria-label=\"Home\">"
         "<svg class=\"vix-brand-mark\" viewBox=\"0 0 36 36\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\">"
         "<defs>"
         "<linearGradient id=\"dl\" x1=\"5\" y1=\"6\" x2=\"18\" y2=\"30\" gradientUnits=\"userSpaceOnUse\">"
         "<stop offset=\"0%\" stop-color=\"#d4fcd4\"/>"
         "<stop offset=\"55%\" stop-color=\"#4ade80\"/>"
         "<stop offset=\"100%\" stop-color=\"#22c55e\"/>"
         "</linearGradient>"
         "<linearGradient id=\"dr\" x1=\"31\" y1=\"6\" x2=\"18\" y2=\"30\" gradientUnits=\"userSpaceOnUse\">"
         "<stop offset=\"0%\" stop-color=\"#22c55e\"/>"
         "<stop offset=\"100%\" stop-color=\"#15803d\"/>"
         "</linearGradient>"
         "</defs>"
         "<polygon points=\"5,6 12,6 18,28 14,28\" fill=\"url(#dl)\"/>"
         "<polygon points=\"31,6 24,6 18,28 22,28\" fill=\"url(#dr)\"/>"
         "<line x1=\"9\" y1=\"16\" x2=\"13.5\" y2=\"29\" stroke=\"#bbf7d0\" stroke-width=\"1.1\" stroke-linecap=\"round\" opacity=\"0.7\"/>"
         "</svg>"
         "<span class=\"vix-brand-name\">Vix</span>"
         "<span class=\"vix-brand-dim\"> API Docs</span>"
         "</a>";

    // Middle: title + sub (filled by JS)
    h += "<div class=\"vix-meta\">"
         "<span class=\"vix-pill\"><b id=\"vix-docs-title\">Loading…</b></span>"
         "<span class=\"vix-pill\" id=\"vix-docs-sub\" style=\"color:var(--muted2)\">Fetching spec</span>"
         "</div>";

    // Right: spec link + version
    h += "<div class=\"vix-topbar-right\">"
         "<a class=\"vix-spec-link\" id=\"vix-openapi-link\" href=\"/openapi.json\" target=\"_blank\" rel=\"noopener\">"
         "<code id=\"vix-openapi-path\">/openapi.json</code>"
         "</a>"
         "<span class=\"vix-ver-pill\" id=\"vix-openapi-version\">v—</span>"
         "</div>";

    h += "</div>"
         "</header>";

    // Swagger container
    h += "<div id=\"swagger-ui\"></div>";

    // ── JS ──
    h += "<script src=\"swagger-ui-bundle.js\"></script>"
         "<script>"
         "(function(){";

    h += "var URL='";
    detail::append_js_escaped(h, openapi_url);
    h += "';";

    h += "function t(id,v){var e=document.getElementById(id);if(e)e.textContent=v}";

    h += "function info(){"
         "t('vix-openapi-path',URL);"
         "var a=document.getElementById('vix-openapi-link');"
         "if(a)a.setAttribute('href',URL);"
         "fetch(URL,{cache:'no-store'})"
         ".then(function(r){if(!r.ok)throw new Error(r.status);return r.json()})"
         ".then(function(j){"
         "t('vix-docs-title',(j&&j.info&&j.info.title)||'Vix API');"
         "t('vix-openapi-version','v'+((j&&j.info&&j.info.version)||'-'));"
         "t('vix-docs-sub','OpenAPI 3.0.3');"
         "})"
         ".catch(function(e){"
         "t('vix-docs-sub','Spec unavailable');"
         "console.error(e)"
         "})"
         "}";

    h += "function mount(){"
         "var el=document.getElementById('swagger-ui');"
         "if(!el)return;"
         "el.innerHTML='';"
         "info();"
         "if(!window.SwaggerUIBundle){console.error('SwaggerUIBundle missing');return}"
         "try{"
         "window.ui=SwaggerUIBundle({"
         "url:URL,"
         "dom_id:'#swagger-ui',"
         "deepLinking:true,"
         "persistAuthorization:true,"
         "displayRequestDuration:true,"
         "defaultModelsExpandDepth:1,"
         "docExpansion:'list'"
         "})"
         "}catch(e){console.error('SwaggerUI init failed',e)}"
         "}";

    h += "if(document.readyState==='loading'){"
         "document.addEventListener('DOMContentLoaded',mount)"
         "}else{mount()}"
         "window.addEventListener('pageshow',mount)";

    h += "})();"
         "</script>";

    h += "</body></html>";

    return h;
  }

} // namespace vix::openapi

#endif // VIX_DOCS_UI_HPP
