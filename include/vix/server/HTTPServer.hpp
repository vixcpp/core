#ifndef VIX_HTTP_SERVER_HPP
#define VIX_HTTP_SERVER_HPP

/**
 * @file HTTPServer.hpp
 * @brief High‑performance HTTP server for Vix.cpp built on Boost.Asio/Beast.
 *
 * @details
 * The `vix::HTTPServer` coordinates network I/O (via Boost.Asio), HTTP parsing
 * (via Boost.Beast), request routing (via `vix::Router`), and execution of user
 * handlers (via a dedicated `ThreadPool`). It is designed to be:
 *
 *  - **Event‑driven**: a single `io_context` dispatches async accept/read/write.
 *  - **Scalable**: multiple I/O threads + a request worker pool.
 *  - **Composable**: a shared `Router` is exposed for route registration.
 *  - **Production‑ready**: explicit lifecycle APIs (`run()`, `stop_async()`,
 *    `join_threads()`), structured logging, and graceful shutdown semantics.
 *
 * ### Threading model
 * - **I/O threads**: N threads (computed by `calculate_io_thread_count()`) run
 *   the `io_context` and execute asynchronous accept/read/write operations.
 * - **Request workers**: application callbacks are delegated to
 *   `vix::ThreadPool` to isolate CPU‑bound work from the I/O loop.
 *
 * ### Lifetime & ownership
 * - `HTTPServer` owns the `io_context_`, the listening `acceptor_`, the
 *   request `ThreadPool`, and the I/O worker threads.
 * - The `Router` is held as a `std::shared_ptr` and exposed via `getRouter()`
 *   so that callers can register routes before `run()`.
 *
 * ### Usage example
 * @code{.cpp}
 * #include <vix/config/Config.hpp>
 * #include <vix/server/HTTPServer.hpp>
 * using namespace Vix;
 *
 * int main() {
 *   Config cfg;
 *   cfg.set("server.port", 8080);
 *   cfg.set("server.address", std::string("0.0.0.0"));
 *
 *   HTTPServer server(cfg);
 *   auto r = server.getRouter();
 *   r->get("/health", [](auto& req, auto& res){ res.text(200, "OK"); });
 *
 *   // Blocking; install a signal handler to call stop_async() from another thread.
 *   server.run();
 *   return 0;
 * }
 * @endcode
 *
 * ### Shutdown contract
 * - Call `stop_async()` from a signal handler / control thread to request
 *   shutdown; then call `join_threads()` to wait for worker termination if your
 *   application requires an explicit join phase.
 *
 * @note Unless stated otherwise, public methods are intended to be invoked from
 *       the main/control thread. `stop_async()` is thread‑safe.
 */

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <boost/regex.hpp>
#include <string>
#include <unordered_map>
#include <iostream>
#include <thread>
#include <memory>
#include <functional>
#include <spdlog/spdlog.h>
#include <vector>
#include <system_error>
#include <boost/system/error_code.hpp>
#include <atomic>
#include <cstring>

#include <vix/http/IRequestHandler.hpp>
#include <vix/router/Router.hpp>
#include <vix/session/Session.hpp>
#include <vix/http/Response.hpp>
#include <vix/config/Config.hpp>
#include <vix/threadpool/ThreadPool.hpp>

#include <vix/executor/IExecutor.hpp>

namespace vix::server
{
    namespace beast = boost::beast;
    namespace http = boost::beast::http;
    namespace net = boost::asio;

    using tcp = net::ip::tcp;

    /// Default number of request worker threads when not specified by config.
    constexpr size_t NUMBER_OF_THREADS = 8;

    /**
     * @class HTTPServer
     * @brief Accepts TCP connections, parses HTTP, routes to handlers, and
     *        orchestrates request execution.
     */
    class HTTPServer
    {
    public:
        /**
         * @brief Construct the server with the provided configuration.
         *
         * Recognized configuration keys (non‑exhaustive):
         *  - `server.address` (string)  — bind address (e.g. "0.0.0.0").
         *  - `server.port`    (integer) — listening port.
         *  - `server.io_threads` (integer, optional) — number of I/O threads;
         *    if absent or <= 0, an appropriate value is computed.
         *  - `server.request_threads` (integer, optional) — size of the worker
         *    pool for user handlers; defaults to `NUMBER_OF_THREADS`.
         */
        explicit HTTPServer(vix::config::Config &config, std::shared_ptr<vix::executor::IExecutor> exec);

        /** @brief Destructor; requests stop and joins owned threads if needed. */
        ~HTTPServer();

        /**
         * @brief Start the server event loop.
         *
         * Initializes the acceptor, starts I/O threads, begins accepting
         * connections, and blocks the calling thread until shutdown is
         * requested via `stop_async()` or an unrecoverable error occurs.
         */
        void run();

        /**
         * @brief Begin asynchronous accept on the listening socket.
         * @pre `init_acceptor()` has completed successfully.
         */
        void start_accept();

        /**
         * @brief Compute the number of I/O threads to run.
         * @return A positive integer based on configuration and hardware
         *         concurrency (never returns 0).
         */
        std::size_t calculate_io_thread_count();

        /**
         * @brief Access the server's router for route registration.
         * @warning Do not mutate routing tables from request threads while the
         *          server is actively handling traffic unless your `Router`
         *          implementation is designed for concurrent mutation.
         */
        std::shared_ptr<vix::router::Router> getRouter() { return router_; }

        /**
         * @brief Periodically collect and log runtime metrics.
         * @details Intended to be run in a background task; exact metrics are
         *          implementation‑defined (e.g., open connections, queue depth,
         *          error rates, latency percentiles if available).
         */
        void monitor_metrics();

        /**
         * @brief Signal an asynchronous, cooperative shutdown.
         *
         * Safe to call from any thread. Cancels outstanding accepts, requests
         * the `io_context_` to stop, and asks the request thread pool to drain
         * queued work. The call returns immediately; use `join_threads()` to
         * wait for completion if desired.
         */
        void stop_async();

        /**
         * @brief Join owned worker threads (I/O + request pool) if they are running.
         * @note Call this after `stop_async()` if your application needs a
         *       deterministic join phase before process exit.
         */
        void join_threads();

        /** @return Whether a stop request has been issued. */
        bool is_stop_requested() const { return stop_requested_.load(); }

        void stop_blocking();

    private:
        /**
         * @brief Create and bind the listening acceptor.
         * @param port TCP port to bind.
         * @throws std::system_error on bind/listen failures.
         */
        void init_acceptor(unsigned short port);

        /**
         * @brief Handle a single client connection lifecycle.
         * @param socket_ptr Accepted TCP socket (shared for safe ownership).
         * @param router     Router to resolve the target handler.
         *
         * Responsible for reading/parsing the HTTP request, invoking the
         * matched handler via the request worker pool, and writing the response
         * back to the client.
         */
        void handle_client(std::shared_ptr<tcp::socket> socket_ptr);
        /** @brief Close the given socket, ignoring benign errors. */
        void close_socket(std::shared_ptr<tcp::socket> socket);

        /** @brief Launch the I/O worker threads that run `io_context_`. */
        void start_io_threads();

    private:
        vix::config::Config &config_;                 //!< Server configuration (non‑owning).
        std::shared_ptr<net::io_context> io_context_; //!< Shared I/O context.
        std::unique_ptr<tcp::acceptor> acceptor_;     //!< Listening socket.
        std::shared_ptr<vix::router::Router> router_; //!< HTTP router for request dispatch.
        std::shared_ptr<vix::executor::IExecutor> executor_;
        std::vector<std::thread> io_threads_;     //!< Threads running `io_context_`.
        std::atomic<bool> stop_requested_{false}; //!< Cooperative stop flag.
        std::chrono::steady_clock::time_point startup_t0_;
    };

} // namespace vix

#endif // VIX_HTTP_SERVER_HPP
