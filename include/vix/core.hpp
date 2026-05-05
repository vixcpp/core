/**
 *
 * @file core.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license thatcan be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_CORE_HPP
#define VIX_CORE_HPP

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include <vix/console.hpp>
#include <vix/print.hpp>
#include <vix/console.hpp>
#include <vix/app/App.hpp>
#include <vix/server/HTTPServer.hpp>
#include <vix/threadpool/ThreadPool.hpp>
#include <vix/router/Router.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <vix/session/Session.hpp>
#include <vix/config/Config.hpp>
#include <vix/http/Status.hpp>

namespace vix
{
  /**
   * @brief Public template namespace alias.
   *
   * This alias exposes the template module under a shorter and cleaner name.
   * It allows users to access template-related types such as Context and Engine
   * without referencing the internal `template_` namespace directly.
   *
   * Example:
   * @code
   * vix::tmpl::Context ctx;
   * ctx.set("title", "Home");
   * @endcode
   */
  namespace tmpl = vix::template_;

  /** @brief Public App alias. */
  using App = vix::App;

  /** @brief Shared pointer to App. */
  using AppPtr = std::shared_ptr<App>;

  /** @brief Shared pointer to HTTP session. */
  using SessionPtr = std::shared_ptr<vix::session::Session>;

  /** @brief HTTP request alias. */
  using Request = vix::http::Request;

  /** @brief HTTP response alias. */
  using Response = vix::http::ResponseWrapper;
} // namespace vix

#endif // VIX_CORE_HPP
