#ifndef VIX_ROUTE_DOC_HPP
#define VIX_ROUTE_DOC_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace vix::router
{
  struct RouteDoc
  {
    // OpenAPI basics
    std::string summary{};
    std::string description{};
    std::vector<std::string> tags{};

    // Example:
    // request_body = { {"content", { {"application/json", { {"schema", {...}} } } } } };
    nlohmann::json request_body = nlohmann::json::object();

    // Map status code -> OpenAPI response object
    // Example:
    // responses["200"] = { {"description","OK"}, {"content", {...}} };
    nlohmann::json responses = nlohmann::json::object();

    // Extra custom extensions for Vix runtime
    // Example:
    // x["x-vix-heavy"] = true;
    nlohmann::json x = nlohmann::json::object();

    bool empty() const noexcept
    {
      return summary.empty() &&
             description.empty() &&
             tags.empty() &&
             request_body.empty() &&
             responses.empty() &&
             x.empty();
    }
  };
}

#endif
