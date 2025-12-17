#ifndef VIX_APP_HPP
#define VIX_APP_HPP

/**
 * @file App.hpp
 * @brief High-level Vix.cpp application entry point combining Config, Router, and HTTPServer.
 */

#include <memory>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/config/Config.hpp>
#include <vix/server/HTTPServer.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/utils/Logger.hpp>

#include <vix/executor/IExecutor.hpp>
#include <vix/experimental/ThreadPoolExecutor.hpp>

namespace vix::router
{
    class Router; // forward declaration pour éviter d'inclure Router.hpp ici
}

namespace vix
{
    namespace http = boost::beast::http;
    using Logger = vix::utils::Logger;

    /**
     * @class App
     * @brief Main application orchestrator for Vix.cpp HTTP services.
     */
    class App
    {
    public:
        using ShutdownCallback = std::function<void()>;

        /** @brief Construct a new App instance with shared Config and Router. */
        App();
        ~App() = default;

        // Non-copyable / non-movable
        App(const App &) = delete;
        App &operator=(const App &) = delete;
        App(App &&) = delete;
        App &operator=(App &&) = delete;

        /**
         * @brief Start the HTTP server.
         *
         * Blocks until a termination signal (SIGINT/SIGTERM) is received,
         * performing graceful shutdown of worker threads and open connections.
         *
         * @param port Optional override of the configured port.
         */
        void run(int port = 8080);

        /**
         * @brief Register a callback invoked during graceful shutdown.
         *
         * Typical usage:
         *   - stopping auxiliary runtimes (e.g. WebSocket App)
         *   - flushing custom metrics, background workers, etc.
         *
         * The callback is executed once, after a stop signal has been received
         * (SIGINT/SIGTERM) and before the HTTP server completes its teardown.
         */
        void set_shutdown_callback(ShutdownCallback cb)
        {
            shutdown_cb_ = std::move(cb);
        }

        // --------------------------------------------------------------
        // Express-like route registration helpers
        // --------------------------------------------------------------
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

        // --------------------------------------------------------------
        // Accessors
        // --------------------------------------------------------------
        /** @return The global Config instance used by this app. */
        vix::config::Config &config() noexcept { return config_; }

        /** @return Shared Router for registering or inspecting routes. */
        std::shared_ptr<vix::router::Router> router() const noexcept { return router_; }

        /** @return Underlying HTTPServer instance handling requests. */
        vix::server::HTTPServer &server() noexcept { return server_; }

        vix::executor::IExecutor &executor() noexcept { return *executor_; }

    private:
        vix::config::Config &config_;                        ///< Global configuration reference
        std::shared_ptr<vix::router::Router> router_;        ///< Shared router (injected into HTTPServer)
        std::shared_ptr<vix::executor::IExecutor> executor_; ///< Executor used by HTTP server
        vix::server::HTTPServer server_;                     ///< Core HTTP server

        ShutdownCallback shutdown_cb_{}; ///< Optional hook executed on shutdown

        /**
         * @brief Internal helper for adding a typed route handler.
         *
         * Wraps the provided handler into a RequestHandler<Handler> adapter,
         * then registers it on the router. Logs registration details.
         */
        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            auto &log = Logger::getInstance();
            if (!router_)
            {
                log.throwError("Router is not initialized in App");
            }

            using Adapter = vix::vhttp::RequestHandler<Handler>;
            auto request_handler = std::make_shared<Adapter>(path, std::move(handler));
            router_->add_route(method, path, request_handler);

            log.logf(Logger::Level::DEBUG,
                     "Route registered",
                     "method", static_cast<int>(method),
                     "path", path.c_str());
        }
    };

} // namespace vix

#endif // VIX_APP_HPP
