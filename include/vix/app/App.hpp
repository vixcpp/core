#ifndef VIX_APP_HPP
#define VIX_APP_HPP

/**
 * @file App.hpp
 * @brief High-level Vix.cpp application entry point combining Config, Router, and HTTPServer.
 *
 * @details
 * The `Vix::App` class provides a simplified, Express-like interface for defining routes
 * and running an HTTP server. It serves as the glue between configuration (`Config`), routing
 * (`Router`), and networking (`HTTPServer`).
 *
 * ### Responsibilities
 * - Initialize and hold shared instances of Config, Router, and HTTPServer.
 * - Register routes for multiple HTTP verbs using a uniform API (`get`, `post`, etc.).
 * - Manage the server lifecycle (`run()`) and handle graceful shutdowns on signals.
 * - Provide runtime access to core subsystems (config, router, server).
 *
 * ### Example usage
 * ```cpp
 * #include <vix/App.hpp>
 * using namespace Vix;
 *
 * int main() {
 *     App app;
 *
 *     app.get("/hello", [](const auto& req, ResponseWrapper& res){
 *         res.text("Hello, World!");
 *     });
 *
 *     app.run(8080);
 *     return 0;
 * }
 * ```
 *
 * ### Thread safety
 * Routes should be registered before calling `run()`. Once running, all
 * route handlers are executed concurrently within the HTTPServer's worker pool.
 */

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

#include <vix/executor/IExecutor.hpp>
#include <vix/experimental/ThreadPoolExecutor.hpp>

namespace Vix
{
    namespace http = boost::beast::http;

    /**
     * @class App
     * @brief Main application orchestrator for Vix.cpp HTTP services.
     */
    class App
    {
    public:
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

        // --------------------------------------------------------------
        // Express-like route registration helpers
        // --------------------------------------------------------------
        template <typename Handler>
        void get(const std::string &path, Handler handler) { add_route(http::verb::get, path, std::move(handler)); }

        template <typename Handler>
        void post(const std::string &path, Handler handler) { add_route(http::verb::post, path, std::move(handler)); }

        template <typename Handler>
        void put(const std::string &path, Handler handler) { add_route(http::verb::put, path, std::move(handler)); }

        template <typename Handler>
        void patch(const std::string &path, Handler handler) { add_route(http::verb::patch, path, std::move(handler)); }

        template <typename Handler>
        void del(const std::string &path, Handler handler) { add_route(http::verb::delete_, path, std::move(handler)); }

        template <typename Handler>
        void head(const std::string &path, Handler handler) { add_route(http::verb::head, path, std::move(handler)); }

        template <typename Handler>
        void options(const std::string &path, Handler handler) { add_route(http::verb::options, path, std::move(handler)); }

        // --------------------------------------------------------------
        // Accessors
        // --------------------------------------------------------------
        /** @return The global Config instance used by this app. */
        Config &config() noexcept { return config_; }
        /** @return Shared Router for registering or inspecting routes. */
        std::shared_ptr<Router> router() const noexcept { return router_; }
        /** @return Underlying HTTPServer instance handling requests. */
        HTTPServer &server() noexcept { return server_; }

    private:
        Config &config_;                 ///< Global configuration reference
        std::shared_ptr<Router> router_; ///< Shared router (injected into HTTPServer)
        std::shared_ptr<Vix::IExecutor> executor_;
        HTTPServer server_; ///< Core HTTP server

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

            auto request_handler = std::make_shared<RequestHandler<Handler>>(path, std::move(handler));
            router_->add_route(method, path, request_handler);

            log.logf(Logger::Level::DEBUG, "Route registered", "method", static_cast<int>(method), "path", path.c_str());
        }
    };

} // namespace Vix

#endif // VIX_APP_HPP