#ifndef VIX_OPENAPI_HPP
#define VIX_OPENAPI_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <boost/beast/http.hpp>

#include <vix/router/Router.hpp>
#include <vix/router/RouteDoc.hpp>

namespace vix::openapi
{
  namespace http = boost::beast::http;

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
    case http::verb::options:
      return "options";
    default:
      return "x-other";
    }
  }

  inline nlohmann::json default_responses()
  {
    return nlohmann::json{
        {"200", {{"description", "OK"}}}};
  }

  inline nlohmann::json build_from_router(
      const vix::router::Router &router,
      std::string title = "Vix API",
      std::string version = "0.0.0")
  {
    nlohmann::json doc;
    doc["openapi"] = "3.0.3";
    doc["info"] = {
        {"title", std::move(title)},
        {"version", std::move(version)}};

    doc["paths"] = nlohmann::json::object();

    for (const auto &r : router.routes())
    {
      auto &pathItem = doc["paths"][r.path];
      const std::string m = method_to_openapi(r.method);

      nlohmann::json op = nlohmann::json::object();

      if (!r.doc.summary.empty())
        op["summary"] = r.doc.summary;
      if (!r.doc.description.empty())
        op["description"] = r.doc.description;
      if (!r.doc.tags.empty())
        op["tags"] = r.doc.tags;

      if (!r.doc.request_body.empty())
        op["requestBody"] = r.doc.request_body;

      if (!r.doc.responses.empty())
        op["responses"] = r.doc.responses;
      else
        op["responses"] = default_responses();

      for (auto it = r.doc.x.begin(); it != r.doc.x.end(); ++it)
        op[it.key()] = it.value();

      pathItem[m] = std::move(op);
    }

    return doc;
  }
}

#endif
