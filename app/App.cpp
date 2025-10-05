#include "App.hpp"

namespace Vix
{
    App::App()
        : config_(Config::getInstance()),
          server_(config_)
    {
        try
        {
            config_.loadConfig();
            router_ = server_.getRouter();

            if (!router_)
            {
                throw std::runtime_error("Failed to get router from HTTPServer");
            }
        }
        catch (const std::exception &e)
        {
            spdlog::critical("Failed to initialize App: {}", e.what());
            throw;
        }
    }

    void App::run(int port)
    {
        try
        {
            config_.setServerPort(port);
            server_.run();
        }
        catch (const std::exception &e)
        {
            spdlog::critical("Critical error while running the server: {}", e.what());
            throw;
        }
        catch (...)
        {
            spdlog::critical("Unknown critical error while running the server");
            throw;
        }
    }
}
