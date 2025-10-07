#include <vix/app/App.hpp>
#include <vix/utils/Env.hpp> // env_bool/env_or
#include <vix/utils/Logger.hpp>

#include <csignal>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace Vix
{
    // ------------------------------------------------------
    // Process-wide state for graceful shutdown on SIGINT
    // ------------------------------------------------------
    static HTTPServer *g_server_ptr = nullptr;
    static std::atomic<bool> g_stop_flag{false};
    static std::mutex g_stop_mutex;
    static std::condition_variable g_stop_cv;

    static void handle_sigint(int)
    {
        auto &log = Logger::getInstance();
        log.log(Logger::Level::INFO, "Received SIGINT, shutting down...");

        g_stop_flag = true;
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
        : config_(Config::getInstance()),
          server_(config_)
    {
        auto &log = Logger::getInstance();

        // Minimal, production-friendly defaults
        log.setLevel(Logger::Level::WARN);
        log.setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        // Toggle async logging by environment (default ON in production)
        // VIX_LOG_ASYNC=0 -> force sync logs
        if (utils::env_bool("VIX_LOG_ASYNC", true))
        {
            log.setAsync(true);
            log.log(Logger::Level::INFO, "Logger initialized in ASYNC mode");
        }
        else
        {
            log.setAsync(false);
            log.log(Logger::Level::INFO, "Logger initialized in SYNC mode");
        }

        // Optional context for better observability (module tag)
        Logger::Context ctx;
        ctx.module = "App";
        log.setContext(ctx);

        try
        {
            config_.loadConfig();
            router_ = server_.getRouter();
            if (!router_)
            {
                log.throwError("Failed to get Router from HTTPServer");
            }
        }
        catch (const std::exception &e)
        {
            log.throwError("Failed to initialize App: {}", e.what());
        }
    }

    // ------------------------------------------------------
    // App::run: start server, install SIGINT handler, wait until stop
    // ------------------------------------------------------
    void App::run(int port)
    {
        auto &log = Logger::getInstance();
        config_.setServerPort(port);

        g_server_ptr = &server_;

        // Register SIGINT only once per process (idempotent enough for typical use)
        std::signal(SIGINT, handle_sigint);

        // Run the HTTP server in a dedicated thread
        std::thread server_thread([this]()
                                  { server_.run(); });

        // Block the main thread until a stop signal is received
        {
            std::unique_lock<std::mutex> lock(g_stop_mutex);
            g_stop_cv.wait(lock, []
                           { return g_stop_flag.load(); });
        }

        // Join server thread
        if (server_thread.joinable())
            server_thread.join();

        // Ensure all worker threads complete before returning
        server_.join_threads();

        log.log(Logger::Level::INFO, "Application shutdown complete");
    }
}
