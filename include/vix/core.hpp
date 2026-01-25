/**
 *
 *  @file core.hpp
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
#ifndef VIX_CORE_HPP
#define VIX_CORE_HPP

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>

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

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace bhttp = boost::beast::http;

namespace vix
{
  using tcp = asio::ip::tcp;

  using AppPtr = std::shared_ptr<App>;
  using SessionPtr = std::shared_ptr<vix::session::Session>;

  using Request = vix::vhttp::Request;
  using Response = vix::vhttp::ResponseWrapper;
} // namespace vix

#endif
