#pragma once
// ======================================================
// vix/core.h
// Public umbrella header for the Vix Core module.
// Exposes HTTP server, router, request/response, middleware,
// session management, config, and app components.
// ======================================================

#include <memory>
#include <string>
#include <vector>
#include <functional>

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
// Session Management
// ----------------------------
#include <vix/session/Session.hpp>

// ----------------------------
// Configuration
// ----------------------------
#include <vix/config/Config.hpp>

// ======================================================
// Namespace & aliases
// ======================================================

namespace Vix
{

    namespace http = boost::beast::http;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    using tcp = net::ip::tcp;

    // JSON
    using json = nlohmann::json;

    using AppPtr = std::shared_ptr<App>;
    using SessionPtr = std::shared_ptr<Session>;
    using ModulePtr = std::shared_ptr<Module>;
} // namespace Vix
