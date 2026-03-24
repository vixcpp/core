/**
 *
 * @file Register.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_OPENAPI_REGISTRY_HPP
#define VIX_OPENAPI_REGISTRY_HPP

#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <vix/router/RouteDoc.hpp>

namespace vix::openapi
{
  /**
   * @brief Route documentation entry registered outside the core HTTP router.
   */
  struct ExtraRouteDoc
  {
    std::string method{};
    std::string path{};
    vix::router::RouteDoc doc{};
  };

  /**
   * @brief Global registry for additional OpenAPI route docs (websocket, modules, etc.).
   */
  class Registry
  {
  public:
    /** @brief Register an extra documented route. */
    static void add(std::string method, std::string path, vix::router::RouteDoc doc)
    {
      auto &self = instance();
      std::lock_guard<std::mutex> lk(self.mu_);
      self.extras_.push_back(ExtraRouteDoc{
          std::move(method),
          std::move(path),
          std::move(doc)});
    }

    /** @brief Return a snapshot of all registered extra route docs. */
    static std::vector<ExtraRouteDoc> snapshot()
    {
      auto &self = instance();
      std::lock_guard<std::mutex> lk(self.mu_);
      return self.extras_;
    }

    /** @brief Clear all registered extra route docs. */
    static void clear()
    {
      auto &self = instance();
      std::lock_guard<std::mutex> lk(self.mu_);
      self.extras_.clear();
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
