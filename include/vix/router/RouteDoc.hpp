/**
 * @file RouteDoc.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_ROUTE_DOC_HPP
#define VIX_ROUTE_DOC_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace vix::router
{
  /** @brief Documentation metadata for a route, used for OpenAPI generation and developer tooling. */
  struct RouteDoc
  {
    /** @brief Short one-line summary describing what the route does. */
    std::string summary{};

    /** @brief Detailed description of the route behavior and usage. */
    std::string description{};

    /** @brief List of tags used to group routes in generated documentation. */
    std::vector<std::string> tags{};

    /** @brief JSON schema or example describing the request body. */
    nlohmann::json request_body = nlohmann::json::object();

    /** @brief JSON object describing possible responses keyed by HTTP status code. */
    nlohmann::json responses = nlohmann::json::object();

    /** @brief Vendor-specific OpenAPI extensions (e.g. "x-vix-*"). */
    nlohmann::json x = nlohmann::json::object();

    /** @brief Return true if no documentation fields are defined. */
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

} // namespace vix::router

#endif // VIX_ROUTE_DOC_HPP
