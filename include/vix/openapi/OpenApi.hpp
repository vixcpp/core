#ifndef VIX_OPENAPI_HPP
#define VIX_OPENAPI_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <boost/beast/http.hpp>

#include <vix/router/Router.hpp>
#include <vix/router/RouteDoc.hpp>
#include <vix/openapi/Registry.hpp>

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

    auto add_doc_to_paths = [&](http::verb method,
                                const std::string &path,
                                const vix::router::RouteDoc &rdoc)
    {
      auto &pathItem = doc["paths"][path];
      const std::string m = method_to_openapi(method);

      nlohmann::json op = nlohmann::json::object();

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
    {
      add_doc_to_paths(r.method, r.path, r.doc);
    }

    // 2) Extra docs registered by other modules (websocket etc.)
    for (const auto &e : vix::openapi::Registry::snapshot())
    {
      add_doc_to_paths(e.method, e.path, e.doc);
    }

    return doc;
  }

} // namespace vix::openapi

#endif // VIX_OPENAPI_HPP
