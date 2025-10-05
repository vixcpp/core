#include "App.hpp"
#include "../utils/Logger.hpp"
#include <csignal>

namespace Vix
{
    static HTTPServer *g_server_ptr = nullptr;

    void handle_sigint(int)
    {
        auto &log = Logger::getInstance();
        log.log(Logger::Level::INFO, "Received SIGINT, shutting down...");
        if (g_server_ptr)
            g_server_ptr->stop();
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

        try
        {
            config_.setServerPort(port);

            g_server_ptr = &server_;
            std::signal(SIGINT, handle_sigint);

            server_.run();

            log.log(Logger::Level::INFO, "Application shutdown complete");
        }
        catch (const std::exception &e)
        {
            log.throwError("Critical error while running the server: {}", e.what());
        }
        catch (...)
        {
            log.throwError("Unknown critical error while running the server");
        }
    }
}
