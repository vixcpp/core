#include "App.hpp"
#include "../utils/Logger.hpp"

namespace Vix
{
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
            server_.run();
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
