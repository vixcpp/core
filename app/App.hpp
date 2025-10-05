#ifndef VIX_APP_HPP
#define VIX_APP_HPP

#include "../config/Config.hpp"
#include "../server/HTTPServer.hpp"
#include "../router/RequestHandler.hpp"
#include <memory>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <csignal>
#include <thread>
#include <spdlog/spdlog.h>

namespace Vix
{
    class App
    {
    public:
        App();
        void run(int port);

        template <typename Handler>
        void get(const std::string &path, Handler handler) { add_route(http::verb::get, path, handler); }

        template <typename Handler>
        void post(const std::string &path, Handler handler) { add_route(http::verb::post, path, handler); }

        template <typename Handler>
        void put(const std::string &path, Handler handler) { add_route(http::verb::put, path, handler); }

        template <typename Handler>
        void del(const std::string &path, Handler handler) { add_route(http::verb::delete_, path, handler); }

    private:
        Config &config_;
        std::shared_ptr<Router> router_;
        HTTPServer server_;

        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            if (!router_)
                throw std::runtime_error("Router is not initialized in App");
            auto request_handler = std::make_shared<RequestHandler<Handler>>(path, std::move(handler));
            router_->add_route(method, path, request_handler);
        }
    };
}

#endif
