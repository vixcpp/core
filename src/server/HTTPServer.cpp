#include <vix/server/HTTPServer.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/timers/interval.hpp>
#include <vix/executor/Metrics.hpp>

/**
 * @file HTTPServer.cpp
 * @brief Implementation details for vix::HTTPServer.
 *
 * @section impl_overview Overview (for contributors)
 * The HTTP server coordinates:
 *  1) TCP accept on a listening socket (Boost.Asio),
 *  2) HTTP read/parse/write (delegated to Session which wraps Boost.Beast),
 *  3) Route resolution (Router), and
 *  4) CPU-bound user handler execution (ThreadPool).
 *
 * The I/O loop remains responsive because user work is offloaded to a
 * dedicated thread-pool. This file documents lifecycle, error-handling,
 * threading, and shutdown semantics.
 *
 *  - Lifecycle: ctor → init_acceptor() → run() → {start_accept(), monitor_metrics(), start_io_threads()} → stop_async() → join_threads() → dtor.
 *  - Threading: N I/O threads run io_context_. Request handling is queued on
 *    request_thread_pool_.
 *  - Shutdown: stop_async() is cooperative and thread-safe; it closes the
 *    acceptor, stops io_context_, and lets the pool drain. Call join_threads()
 *    from the control thread for deterministic shutdown.
 *
 * @note Public API and behavioral contract live in the header docs. This file
 *       focuses on maintainers' notes and rationale.
 */

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace vix::server
{
    using Logger = vix::utils::Logger;
    /**
     * @brief Pin the current thread to a CPU core on Linux.
     *
     * @details Useful on systems where CPU affinity can improve cache locality
     * and reduce context switches for high-throughput servers. No-ops on
     * non-Linux platforms.
     *
     * @param thread_id 0-based index of the worker attempting affinity.
     */
    void set_affinity(int thread_id)
    {
#ifdef __linux__
        unsigned int hc = std::thread::hardware_concurrency();
        if (hc == 0)
            hc = 1; // Fallback to 1 when unknown
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(thread_id % hc, &cpuset);
        // Best-effort: failure is non-fatal.
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    }

    /**
     * @brief Construct the HTTP server and install default routing + safety rails.
     *
     * @throws std::invalid_argument when port is outside [1024, 65535].
     * @throws std::system_error for bind/listen errors inside init_acceptor().
     *
     * Implementation notes:
     *  - Creates a shared io_context_ to be driven by start_io_threads().
     *  - Instantiates a Router with a JSON 404 fallback, including proper HEAD
     *    semantics (no body, correct Content-Length).
     *  - Initializes the acceptor on the configured port.
     */
    HTTPServer::HTTPServer(vix::config::Config &config, std::shared_ptr<vix::executor::IExecutor> exec)
        : config_(config),
          io_context_(std::make_shared<net::io_context>()),
          acceptor_(nullptr),
          router_(std::make_shared<vix::router::Router>()),
          executor_(std::move(exec)),
          io_threads_(),
          stop_requested_(false)
    {
        auto &log = Logger::getInstance();
        try
        {
            // ---- Router: structured JSON 404 fallback (incl. HEAD handling) ----
            router_->setNotFoundHandler(
                [](const http::request<http::string_body> &req,
                   http::response<http::string_body> &res)
                {
                    res.result(http::status::not_found);

                    if (req.method() == http::verb::head)
                    {
                        // HEAD → no body, consistent headers
                        res.set(http::field::content_type, "application/json");
                        res.set(http::field::connection, "close");
                        res.body().clear();
                        res.prepare_payload(); // Content-Length: 0
                        return;
                    }

                    nlohmann::json j{
                        {"error", "Route not found"},
                        {"hint", "Check path, method, or API version"},
                        {"method", std::string(req.method_string())},
                        {"path", std::string(req.target())}};

                    vix::vhttp::Response::json_response(res, j, res.result());
                    // Force close to avoid clients lingering for keep-alive.
                    res.set(http::field::connection, "close");
                    res.prepare_payload();
                });

            // ---- Validate and bind port ----
            int port = config_.getServerPort();
            if (port < 1024 || port > 65535)
            {
                log.log(Logger::Level::ERROR, "Server port {} out of range (1024-65535)", port);
                throw std::invalid_argument("Invalid port number");
            }

            init_acceptor(static_cast<unsigned short>(port));

            log.log(Logger::Level::INFO,
                    "Server request timeout set to {} ms",
                    config_.getRequestTimeout());
        }
        catch (const std::exception &e)
        {
            // Propagate after logging to aid early-boot diagnostics
            log.log(Logger::Level::ERROR, "Error initializing HTTPServer: {}", e.what());
            throw;
        }
    }

    /**
     * @brief Dtor: currently default. Ensure control path calls stop_async() and join_threads().
     */
    HTTPServer::~HTTPServer() = default;

    /**
     * @brief Create/bind/listen the acceptor.
     * @throws std::system_error on any socket operation failure.
     *
     * Rationale: we fail-fast during boot rather than defer bind/listen errors
     * into the accept loop.
     */
    void HTTPServer::init_acceptor(unsigned short port)
    {
        auto &log = Logger::getInstance();
        acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);
        boost::system::error_code ec;

        tcp::endpoint endpoint(tcp::v4(), port);
        acceptor_->open(endpoint.protocol(), ec);
        if (ec)
            throw std::system_error(ec, "open acceptor");

        acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            throw std::system_error(ec, "reuse_address");

        acceptor_->bind(endpoint, ec);
        if (ec)
        {
            if (ec == boost::system::errc::address_in_use)
            {
                throw std::system_error(
                    ec,
                    "bind acceptor: address already in use. "
                    "Another process is listening on this port.");
            }
            throw std::system_error(ec, "bind acceptor");
        }

        acceptor_->listen(boost::asio::socket_base::max_connections, ec);
        if (ec)
            throw std::system_error(ec, "listen acceptor");

        log.log(Logger::Level::INFO, "Acceptor initialized on port {}", port);
    }

    /**
     * @brief Launch I/O workers that drive io_context_->run().
     *
     * Each thread optionally sets CPU affinity (Linux only) and then runs the
     * I/O loop. Exceptions inside run() are logged and the thread exits
     * gracefully.
     */
    void HTTPServer::start_io_threads()
    {
        auto &log = Logger::getInstance();
        std::size_t num_threads = static_cast<std::size_t>(calculate_io_thread_count());

        for (std::size_t i = 0; i < num_threads; ++i)
        {
            io_threads_.emplace_back(
                [this, i, &log]()
                {
                    try
                    {
                        set_affinity(static_cast<int>(i));
                        io_context_->run();
                    }
                    catch (const std::exception &e)
                    {
                        log.log(Logger::Level::ERROR, "Error in io_context thread {}: {}", i, e.what());
                    }
                    log.log(Logger::Level::INFO, "IO thread {} finished", i);
                });
        }
    }

    /**
     * @brief Bring the server online.
     *
     * Sequence:
     *  1) start_accept()  — install the accept-loop callback,
     *  2) monitor_metrics() — schedule periodic pool metrics logging,
     *  3) start_io_threads() — spawn I/O workers that execute the loop.
     *
     * @note run() is non-blocking here because the I/O is driven by background
     *       threads. The control thread can install signal handlers and wait,
     *       or proceed to do other coordination before calling stop_async().
     */
    void HTTPServer::run()
    {
        start_accept();
        monitor_metrics();
        start_io_threads();
    }

    /**
     * @brief Compute how many I/O threads to spawn.
     *
     * Heuristic: max(1, hardware_concurrency/2). This balances context-switch
     * pressure with ability to parallelize kernel I/O completions.
     */
    int HTTPServer::calculate_io_thread_count()
    {
        unsigned int hc = std::thread::hardware_concurrency();
        return std::max(1u, hc ? hc / 2 : 1u);
    }

    /**
     * @brief Install an async_accept loop that dispatches sessions to the pool.
     *
     * On each accept:
     *  - If success and not stopping, queue a task on request_thread_pool_ to
     *    handle the newly accepted socket via handle_client().
     *  - Immediately re-arm accept for the next connection unless stopping.
     *
     * @warning The accept callback runs on an I/O thread; heavy work must be
     *          delegated to the pool (as done here) to keep the I/O loop light.
     */
    void HTTPServer::start_accept()
    {
        auto socket = std::make_shared<tcp::socket>(*io_context_);
        acceptor_->async_accept(*socket, [this, socket](boost::system::error_code ec)
                                {
            if (!ec && !stop_requested_)
            {
                auto timeout = std::chrono::milliseconds(config_.getRequestTimeout());
              executor_->post([this, socket]() {
                    handle_client(socket, router_);
              }, vix::executor::TaskOptions{.priority = 1, .timeout = timeout});
            }
            if (!stop_requested_) start_accept(); });
    }

    /**
     * @brief Per-connection session lifecycle.
     *
     * Delegates to Session (which owns the Beast HTTP read/write loop) and
     * eventually calls Router::handle_request(). The NotFound handler installed
     * in the constructor guarantees a consistent JSON 404 for unmatched routes.
     */
    void HTTPServer::handle_client(std::shared_ptr<tcp::socket> socket_ptr, std::shared_ptr<vix::router::Router> router)
    {
        auto session = std::make_shared<vix::session::Session>(socket_ptr, *router);
        session->run();
    }

    /**
     * @brief Best-effort socket close utility.
     */
    void HTTPServer::close_socket(std::shared_ptr<tcp::socket> socket)
    {
        boost::system::error_code ec;
        socket->shutdown(tcp::socket::shutdown_both, ec);
        socket->close(ec);
    }

    /**
     * @brief Request cooperative shutdown from any thread.
     *
     * Steps:
     *  1) Mark stop_requested_,
     *  2) Close acceptor_ (unblocks async_accept),
     *  3) Stop io_context_ (lets run() exit on all I/O threads),
     *  4) The request pool is expected to drain queued work.
     */
    void HTTPServer::stop_async()
    {
        stop_requested_ = true;
        if (acceptor_ && acceptor_->is_open())
            acceptor_->close();
        io_context_->stop();
    }

    void HTTPServer::stop_blocking()
    {
        executor_->wait_idle();
        join_threads();
    }

    /**
     * @brief Join I/O worker threads. Call from the control thread after stop_async().
     */
    void HTTPServer::join_threads()
    {
        for (auto &t : io_threads_)
            if (t.joinable())
                t.join();
    }

    /**
     * @brief Periodically log request thread-pool metrics.
     *
     * Uses threadpool::ThreadPool::periodicTask to schedule a 5-second interval reporter.
     * The exact metrics structure is defined by threadpool::ThreadPool::getMetrics().
     */
    void HTTPServer::monitor_metrics()
    {
        vix::timers::interval(*executor_, std::chrono::seconds(5), [this]()
                              {
        const auto m = executor_->metrics();
        Logger::getInstance().log(Logger::Level::DEBUG,
            "Executor Metrics -> Pending: {}, Active: {}, TimedOut: {}",
            m.pending, m.active, m.timed_out); });
    }

} // namespace vix
