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
#include <vix/threadpool/ThreadPool.hpp>

// ----------------------------
// Routing
// ----------------------------
#include <vix/router/Router.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/RequestHandler.hpp>

// ----------------------------
// Session
// ----------------------------
#include <vix/session/Session.hpp>

// ----------------------------
// Configuration
// ----------------------------
#include <vix/config/Config.hpp>

#include <vix/http/Status.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace bhttp = boost::beast::http;

namespace vix
{

    // Types pratiques
    using tcp = asio::ip::tcp;

    // Convenience aliases
    using AppPtr = std::shared_ptr<App>;
    using SessionPtr = std::shared_ptr<vix::session::Session>;

} // namespace vix
