#ifndef VIX_APP_HPP
#define VIX_APP_HPP

// -----------------------------
// Vix.cpp - Core App Interface
// -----------------------------
// The App class wires configuration, HTTP server, routing, and signal handling.
// It exposes a minimal Express-like API (get/post/put/del) and delegates to the
// underlying Router/HTTPServer at runtime.

#include "../config/Config.hpp"
#include "../server/HTTPServer.hpp"
#include "../router/RequestHandler.hpp"

#include <memory>
#include <functional>
#include <stdexcept>

// Use Vix Logger, do not include spdlog directly from public headers
#include <vix/utils/Logger.hpp>

namespace Vix
{
    class App
    {
    public:
        App();
        void run(int port);

        template <typename Handler>
        void get(const std::string &path, Handler handler)
        {
            add_route(http::verb::get, path, std::move(handler));
        }

        template <typename Handler>
        void post(const std::string &path, Handler handler)
        {
            add_route(http::verb::post, path, std::move(handler));
        }

        template <typename Handler>
        void put(const std::string &path, Handler handler)
        {
            add_route(http::verb::put, path, std::move(handler));
        }

        template <typename Handler>
        void del(const std::string &path, Handler handler)
        {
            add_route(http::verb::delete_, path, std::move(handler));
        }

    private:
        Config &config_;
        std::shared_ptr<Router> router_;
        HTTPServer server_;

        // Adds a route to the router. This is header-only for convenience,
        // but remains minimal to keep compile times reasonable.
        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            auto &log = Logger::getInstance();
            if (!router_)
            {
                log.throwError("Router is not initialized in App");
            }

            auto request_handler = std::make_shared<RequestHandler<Handler>>(path, std::move(handler));
            router_->add_route(method, path, request_handler);

            log.logf(Logger::Level::DEBUG, "Route registered",
                     "method", static_cast<int>(method),
                     "path", path.c_str());
        }
    };
}

#endif // VIX_APP_HPP
