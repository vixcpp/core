#ifndef VIX_APP_HPP
#define VIX_APP_HPP

// -----------------------------
// Vix.cpp - Core App Interface
// -----------------------------
// The App class wires configuration, HTTP server, routing, and signal handling.
// It exposes a minimal Express-like API (get/post/put/patch/del/head/options)
// and delegates to the underlying Router/HTTPServer at runtime.

#include <memory>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/config/Config.hpp>
#include <vix/server/HTTPServer.hpp>
#include <vix/router/RequestHandler.hpp>
#include <vix/utils/Logger.hpp>

namespace Vix
{
    namespace http = boost::beast::http;

    class App
    {
    public:
        App();
        ~App() = default;

        App(const App &) = delete;
        App &operator=(const App &) = delete;
        App(App &&) = delete;
        App &operator=(App &&) = delete;

        // Start the HTTP server, block until SIGINT/SIGTERM triggers a graceful stop
        void run(int port = 8080);

        // ----------- Express-like helpers -----------
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
        void patch(const std::string &path, Handler handler)
        {
            add_route(http::verb::patch, path, std::move(handler));
        }

        template <typename Handler>
        void del(const std::string &path, Handler handler)
        {
            add_route(http::verb::delete_, path, std::move(handler));
        }

        template <typename Handler>
        void head(const std::string &path, Handler handler)
        {
            add_route(http::verb::head, path, std::move(handler));
        }

        template <typename Handler>
        void options(const std::string &path, Handler handler)
        {
            add_route(http::verb::options, path, std::move(handler));
        }

        // Accessors
        Config &config() noexcept { return config_; }
        std::shared_ptr<Router> router() const noexcept { return router_; }
        HTTPServer &server() noexcept { return server_; }

    private:
        Config &config_;
        std::shared_ptr<Router> router_;
        HTTPServer server_;

        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            auto &log = Logger::getInstance();
            if (!router_)
            {
                log.throwError("Router is not initialized in App");
            }

            auto request_handler =
                std::make_shared<RequestHandler<Handler>>(path, std::move(handler));
            router_->add_route(method, path, request_handler);

            // Structured log helper (already present in your Logger)
            log.logf(Logger::Level::DEBUG, "Route registered",
                     "method", static_cast<int>(method),
                     "path", path.c_str());
        }
    };

} // namespace Vix

#endif // VIX_APP_HPP
