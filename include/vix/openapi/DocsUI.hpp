#ifndef VIX_DOCS_UI_HPP
#define VIX_DOCS_UI_HPP

#include <string>

namespace vix::openapi
{
  inline std::string swagger_ui_html(std::string openapi_url = "/openapi.json")
  {
    std::string html;
    html.reserve(2048);

    html += "<!doctype html><html><head><meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Vix Docs</title>";
    html += "<link rel=\"stylesheet\" href=\"https://unpkg.com/swagger-ui-dist@5/swagger-ui.css\">";
    html += "</head><body>";
    html += "<div id=\"swagger-ui\"></div>";
    html += "<script src=\"https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js\"></script>";
    html += "<script>";
    html += "window.onload = () => {";
    html += "SwaggerUIBundle({ url: '" + openapi_url + "', dom_id: '#swagger-ui' });";
    html += "};";
    html += "</script>";
    html += "</body></html>";

    return html;
  }
}

#endif
