#include <vix/app/App.hpp>
#include <vix/utils/Env.hpp> // env_bool / env_or
#include <vix/utils/Logger.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/router/Router.hpp>
#include <boost/beast/http.hpp>

#include <csignal>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>

/**
 * @file App.cpp
 * @brief Implementation notes for vix::App (maintainers-focused).
 *
 * Responsibilities:
 *  - Initialize logging (pattern, level, async mode via env).
 *  - Load configuration and wire `Router` from `HTTPServer`.
 *  - Install POSIX signal handlers (SIGINT/SIGTERM) for graceful shutdown.
 *  - Coordinate server thread run/stop/join lifecycle.
 *
 * Shutdown model:
 *  - Signals trigger `handle_stop_signal()` which:
 *     1) sets a process-wide stop flag and notifies a condition variable,
 *     2) calls `HTTPServer::stop_async()` (non-blocking) via a global pointer.
 *  - `App::run()` waits on the CV, joins the server thread, then calls
 *    `HTTPServer::stop_blocking()` to ensure a clean teardown.
 */

namespace
{
    vix::utils::Logger::Level parse_log_level_from_env()
    {
        using Level = vix::utils::Logger::Level;

        const std::string raw = vix::utils::env_or("VIX_LOG_LEVEL", std::string{"warn"});
        std::string s;
        s.reserve(raw.size());
        for (char c : raw)
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

        if (s == "trace")
            return Level::TRACE;
        if (s == "debug")
            return Level::DEBUG;
        if (s == "info")
            return Level::INFO;
        if (s == "warn" || s == "warning")
            return Level::WARN;
        if (s == "error")
            return Level::ERROR;
        if (s == "critical")
            return Level::CRITICAL;

        return Level::WARN;
    }

    std::size_t compute_executor_threads()
    {
        auto hc = std::thread::hardware_concurrency();
        if (hc == 0)
            hc = 4;
        return hc;
    }

    // Route de bench simple : GET /bench -> "OK"
    void register_bench_route(vix::router::Router &router)
    {
        namespace http = boost::beast::http;

        auto handlerLambda =
            [](vix::vhttp::Request &req,
               vix::vhttp::ResponseWrapper &res)
        {
            (void)req; // aucun usage pour le moment
            res.ok().text("OK");
            // équivalent à:
            // res.status(http::status::ok).text("OK");
        };

        using BenchHandler = vix::vhttp::RequestHandler<decltype(handlerLambda)>;
        auto handlerPtr = std::make_shared<BenchHandler>("/bench", handlerLambda);

        router.add_route(http::verb::get, "/bench", handlerPtr);
    }

} // namespace

namespace vix
{
    using Logger = vix::utils::Logger;

    // ------------------------------------------------------
    // Process-wide state for graceful shutdown on SIGINT/SIGTERM
    // ------------------------------------------------------
    static vix::server::HTTPServer *g_server_ptr = nullptr;
    static std::atomic<bool> g_stop_flag{false};
    static std::mutex g_stop_mutex;
    static std::condition_variable g_stop_cv;

    /**
     * @brief POSIX signal handler for coordinated shutdown.
     *
     * Notes:
     *  - Minimal work inside the handler: set atomic flag, notify CV,
     *    request server stop via non-blocking API.
     *  - Logging is best-effort; avoid heavy operations here.
     */
    static void handle_stop_signal(int)
    {
        auto &log = Logger::getInstance();
        log.log(Logger::Level::INFO, "Received stop signal, shutting down...");

        g_stop_flag.store(true);
        g_stop_cv.notify_one();

        if (g_server_ptr)
        {
            // Non-blocking stop; worker threads will be joined in App::run
            g_server_ptr->stop_async();
        }
    }

    // ------------------------------------------------------
    // App: configure logger, load config, acquire router/server
    // ------------------------------------------------------
    App::App()
        : config_(vix::config::Config::getInstance()),
          router_(nullptr),
          // 1) créer un executor concret (ThreadPoolExecutor) — valeurs par défaut raisonnables
          executor_(std::make_shared<vix::experimental::ThreadPoolExecutor>(
              compute_executor_threads(), // threads
              compute_executor_threads(), // maxThreads = threads → pas d'élasticité
              /*defaultPriority*/ 1)),
          server_(config_, executor_)
    {
        auto &log = Logger::getInstance();

        // 1) Pattern commun (console + fichier)
        log.setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        // 2) Niveau de log via VIX_LOG_LEVEL (trace|debug|info|warn|error|critical)
        const auto level = parse_log_level_from_env();
        log.setLevel(level);

        // 3) Async / sync via VIX_LOG_ASYNC (par défaut: true)
        if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
        {
            log.setAsync(true);
            log.log(Logger::Level::DEBUG, "Logger initialized in ASYNC mode");
        }
        else
        {
            log.setAsync(false);
            log.log(Logger::Level::DEBUG, "Logger initialized in SYNC mode");
        }

        // 4) Contexte module
        Logger::Context ctx;
        ctx.module = "App";
        log.setContext(ctx);

        try
        {
            // Charge la config (fichier + env)
            config_.loadConfig();

            // Récupère le router depuis HTTPServer
            router_ = server_.getRouter();
            if (!router_)
            {
                log.throwError("Failed to get Router from HTTPServer");
            }

            // 🔹 Enregistrer la route /bench une fois pour toutes au démarrage
            register_bench_route(*router_);
        }
        catch (const std::exception &e)
        {
            log.throwError("Failed to initialize App: {}", e.what());
        }
    }

    // ------------------------------------------------------
    // App::run: start server, install signal handlers, wait until stop
    // ------------------------------------------------------
    void App::run(int port)
    {
        auto &log = Logger::getInstance();

        // 1) Configure the port (already validated in Config::setServerPort)
        config_.setServerPort(port);

        // 2) Expose server pointer to the signal handler
        g_server_ptr = &server_;

        // 3) Set up SIGINT / SIGTERM handlers
        std::signal(SIGINT, handle_stop_signal);
#ifdef SIGTERM
        std::signal(SIGTERM, handle_stop_signal);
#endif

        // 4) Start the server (run() is non-blocking)
        std::thread server_thread([this]()
                                  { server_.run(); });

        // 5) Wait for a stop signal (from handle_stop_signal)
        {
            std::unique_lock<std::mutex> lock(g_stop_mutex);
            g_stop_cv.wait(lock, []
                           { return g_stop_flag.load(std::memory_order_relaxed); });
        }

        // 6) Optional external shutdown hook (e.g. WebSocket runtime)
        if (shutdown_cb_)
        {
            try
            {
                shutdown_cb_();
            }
            catch (const std::exception &e)
            {
                // fmt-style logging: format string literal + args
                log.log(Logger::Level::ERROR,
                        "Shutdown callback threw: {}",
                        e.what());
            }
            catch (...)
            {
                log.log(Logger::Level::ERROR,
                        "Shutdown callback threw unknown exception");
            }
        }

        // 7) Graceful HTTP shutdown sequence (idempotent)
        server_.stop_async();
        server_.stop_blocking(); // stop periodic tasks + waitUntilIdle + join I/O threads

        // 8) Join the thread that started run()
        if (server_thread.joinable())
            server_thread.join();

        // 9) Cleanup
        g_server_ptr = nullptr;

        log.log(Logger::Level::INFO, "Application shutdown complete");
    }

} // namespace vix
