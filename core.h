#pragma once

// ======================================================
// vix/core.h
// Main public header for the core module of vix.cpp
// Exposes HTTP server, router, request/response, middleware,
// session management, JSON utilities, and core app components.
// ======================================================

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

// ----------------------------
// App
// ----------------------------
#include "app/App.hpp"
#include "app/Module.hpp"

// ----------------------------
// HTTP Server
// ----------------------------
#include "server/HTTPServer.hpp"
#include "server/ThreadPool.hpp"

// ----------------------------
// Routing
// ----------------------------
#include "router/Router.hpp"
#include "router/IRequestHandler.hpp"
#include "router/RouteManager.hpp"
#include "router/SimpleRequestHandler.hpp"
#include "router/DynamicRequestHandler.hpp"
#include "router/UnifiedRequestHandler.hpp"

// ----------------------------
// Middleware
// ----------------------------
#include "middleware/Middleware.hpp"
#include "middleware/MiddlewareContext.hpp"
#include "middleware/MiddlewarePipeline.hpp"

// ----------------------------
// Session Management
// ----------------------------
#include "session/Session.hpp"

// ----------------------------
// Utilities
// ----------------------------
#include "utils/JsonHelper.hpp"
#include "utils/ErrorUtils.hpp"
#include "utils/StringUtils.hpp"

// ----------------------------
// Configuration
// ----------------------------
#include "config/Config.hpp"

// ======================================================
// Namespace
// ======================================================

namespace Vix
{

    // Boost aliases
    namespace http = boost::beast::http;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    using tcp = net::ip::tcp;

    // JSON alias
    using json = nlohmann::json;

    // Expose core types to simplify usage
    using AppPtr = std::shared_ptr<App>;
    using SessionPtr = std::shared_ptr<Session>;
    using ModulePtr = std::shared_ptr<Module>;

} // namespace Vix
