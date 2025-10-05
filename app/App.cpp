#include "App.hpp"
#include "../utils/Logger.hpp"
#include <csignal>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace Vix
{
    static HTTPServer *g_server_ptr = nullptr;
    static std::atomic<bool> stop_flag(false);
    static std::mutex stop_mutex;
    static std::condition_variable stop_cv;

    void handle_sigint(int)
    {
        auto &log = Logger::getInstance();
        log.log(Logger::Level::INFO, "Received SIGINT, shutting down...");

        stop_flag = true;
        stop_cv.notify_one();

        if (g_server_ptr)
        {
            g_server_ptr->stop_async();
        }
    }

    App::App()
        : config_(Config::getInstance()),
          server_(config_)
    {
        auto &log = Logger::getInstance();
        log.setLevel(Logger::Level::WARN);

        try
        {
            config_.loadConfig();
            router_ = server_.getRouter();

            if (!router_)
            {
                log.throwError("Failed to get router from HTTPServer");
            }
        }
        catch (const std::exception &e)
        {
            log.throwError("Failed to initialize App: {}", e.what());
        }
    }

    void App::run(int port)
    {
        auto &log = Logger::getInstance();
        config_.setServerPort(port);

        g_server_ptr = &server_;
        std::signal(SIGINT, handle_sigint);

        std::thread server_thread([this]()
                                  { server_.run(); });

        {
            std::unique_lock<std::mutex> lock(stop_mutex);
            stop_cv.wait(lock, []
                         { return stop_flag.load(); });
        }

        if (server_thread.joinable())
            server_thread.join();

        server_.join_threads();

        log.log(Logger::Level::INFO, "Application shutdown complete");
    }
}
