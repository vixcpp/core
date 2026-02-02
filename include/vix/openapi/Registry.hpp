/**
 *
 *  @file Registry.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#ifndef VIX_OPENAPI_REGISTRY_HPP
#define VIX_OPENAPI_REGISTRY_HPP

#include <mutex>
#include <vector>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/router/RouteDoc.hpp>

namespace vix::openapi
{
  namespace http = boost::beast::http;

  struct ExtraRouteDoc
  {
    http::verb method{};
    std::string path{};
    vix::router::RouteDoc doc{};
  };

  class Registry
  {
  public:
    static void add(http::verb method, std::string path, vix::router::RouteDoc doc)
    {
      auto &self = instance();
      std::lock_guard<std::mutex> lk(self.mu_);
      self.extras_.push_back(ExtraRouteDoc{method, std::move(path), std::move(doc)});
    }

    static std::vector<ExtraRouteDoc> snapshot()
    {
      auto &self = instance();
      std::lock_guard<std::mutex> lk(self.mu_);
      return self.extras_;
    }

  private:
    Registry() = default;

    static Registry &instance()
    {
      static Registry g;
      return g;
    }

  private:
    std::mutex mu_{};
    std::vector<ExtraRouteDoc> extras_{};
  };

} // namespace vix::openapi

#endif // VIX_OPENAPI_REGISTRY_HPP
