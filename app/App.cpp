#include "App.hpp"
#include "../utils/Logger.hpp"
#include <csignal>
#include <thread>

namespace Vix
{
    static HTTPServer *g_server_ptr = nullptr;

    void handle_sigint(int)
    {
        auto &log = Logger::getInstance();
        log.log(Logger::Level::INFO, "Received SIGINT, shutting down...");
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

        server_thread.join();

        server_.join_threads();

        log.log(Logger::Level::INFO, "Application shutdown complete");
    }

}
