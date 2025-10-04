#ifndef VIX_APP_HPP
#define VIX_APP_HPP

#include "../config/Config.hpp"
#include "../server/HTTPServer.hpp"
#include "../router/UnifiedRequestHandler.hpp"

#include <memory>
#include <functional>
#include <spdlog/spdlog.h>

namespace Vix
{
    class App
    {
    public:
        App();
        void run(int port);

        template <typename Handler>
        void get(const std::string &path, Handler handler)
        {
            add_route(http::verb::get, path, handler);
        }

        template <typename Handler>
        void post(const std::string &path, Handler handler)
        {
            add_route(http::verb::post, path, handler);
        }

    private:
        Config &config_;
        std::shared_ptr<Router> router_;
        HTTPServer server_;

        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            if (!router_)
            {
                spdlog::error("Router is not initialized!");
                throw std::runtime_error("ROuter is not initialized in App");
            }

            router_->add_route(
                method,
                path,
                std::static_pointer_cast<IRequestHandler>(
                    std::make_shared<UnifiedRequestHandler>(handler)));
        }
    };
}

#endif