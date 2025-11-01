#pragma once
// ======================================================
// vix/core.h - Public umbrella header for Vix Core
// ======================================================

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>

// ----------------------------
// App
// ----------------------------
#include <vix/app/App.hpp>

// ----------------------------
// HTTP Server
// ----------------------------
#include <vix/server/HTTPServer.hpp>
#include <vix/server/ThreadPool.hpp>

// ----------------------------
// Routing
// ----------------------------
#include <vix/router/Router.hpp>
#include <vix/router/IRequestHandler.hpp>
#include <vix/router/RequestHandler.hpp>

// ----------------------------
// Session
// ----------------------------
#include <vix/session/Session.hpp>

// ----------------------------
// Configuration
// ----------------------------
#include <vix/config/Config.hpp>

namespace vix
{

    // Expose some useful Boost.Beast / Asio aliases
    namespace http = boost::beast::http;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    using tcp = net::ip::tcp;

    // Convenience aliases
    using AppPtr = std::shared_ptr<App>;
    using SessionPtr = std::shared_ptr<vix::session::Session>;

} // namespace vix
