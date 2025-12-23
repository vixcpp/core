#ifndef VIX_APP_HPP
#define VIX_APP_HPP

/**
 * @file App.hpp
 * @brief High-level Vix.cpp application entry point combining Config, Router, and HTTPServer.
 */

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <boost/beast/http.hpp>

#include <vix/config/Config.hpp>
#include <vix/server/HTTPServer.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/utils/Logger.hpp>

#include <vix/executor/IExecutor.hpp>
#include <vix/experimental/ThreadPoolExecutor.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>

namespace vix::router
{
    class Router; // forward decl
}

namespace vix
{
    namespace http = boost::beast::http;
    using Logger = vix::utils::Logger;

    class App
    {
    public:
        using ShutdownCallback = std::function<void()>;
        using ListenCallback = std::function<void(const vix::utils::ServerReadyInfo &)>;
        using ListenPortCallback = std::function<void(int)>;

        App();
        explicit App(std::shared_ptr<vix::executor::IExecutor> executor);
        ~App();
        App(const App &) = delete;
        App &operator=(const App &) = delete;
        App(App &&) = delete;
        App &operator=(App &&) = delete;
        void run(int port = 8080);
        void listen(int port = 8080, ListenCallback on_listen = {});
        void wait();
        void close();
        void set_shutdown_callback(ShutdownCallback cb)
        {
            shutdown_cb_ = std::move(cb);
        }

        template <typename Handler>
        void get(const std::string &path, Handler handler)
        {
            add_route(http::verb::get, path, std::move(handler));
        }

        template <typename Handler>
        void post(const std::string &path, Handler handler)
        {
            add_route(http::verb::post, path, std::move(handler));
        }

        template <typename Handler>
        void put(const std::string &path, Handler handler)
        {
            add_route(http::verb::put, path, std::move(handler));
        }

        template <typename Handler>
        void patch(const std::string &path, Handler handler)
        {
            add_route(http::verb::patch, path, std::move(handler));
        }

        template <typename Handler>
        void del(const std::string &path, Handler handler)
        {
            add_route(http::verb::delete_, path, std::move(handler));
        }

        template <typename Handler>
        void head(const std::string &path, Handler handler)
        {
            add_route(http::verb::head, path, std::move(handler));
        }

        template <typename Handler>
        void options(const std::string &path, Handler handler)
        {
            add_route(http::verb::options, path, std::move(handler));
        }

        vix::config::Config &config() noexcept { return config_; }
        std::shared_ptr<vix::router::Router> router() const noexcept { return router_; }
        vix::server::HTTPServer &server() noexcept { return server_; }
        vix::executor::IExecutor &executor() noexcept { return *executor_; }
        bool is_running() const noexcept { return started_.load(std::memory_order_relaxed); }
        void request_stop_from_signal() noexcept;
        void listen_port(int port, ListenPortCallback cb = {});
        void setDevMode(bool v) { dev_mode_ = v; }
        bool isDevMode() const { return dev_mode_; }

    private:
        vix::config::Config &config_;
        std::shared_ptr<vix::router::Router> router_;
        std::shared_ptr<vix::executor::IExecutor> executor_;
        vix::server::HTTPServer server_;
        ShutdownCallback shutdown_cb_{};
        std::thread server_thread_;
        std::atomic<bool> started_{false};
        std::atomic<bool> stop_requested_{false};
        std::mutex stop_mutex_;
        std::condition_variable stop_cv_;
        bool dev_mode_ = {false};

        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            auto &log = Logger::getInstance();
            if (!router_)
            {
                log.throwError("Router is not initialized in App");
            }

            using Adapter = vix::vhttp::RequestHandler<Handler>;
            auto request_handler = std::make_shared<Adapter>(path, std::move(handler));
            router_->add_route(method, path, request_handler);

            log.logf(Logger::Level::DEBUG,
                     "Route registered",
                     "method", static_cast<int>(method),
                     "path", path.c_str());
        }
    };

} // namespace vix

#endif // VIX_APP_HPP
