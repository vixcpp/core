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
#include <vector>
#include <filesystem>

#include <boost/beast/http.hpp>

#include <vix/config/Config.hpp>
#include <vix/server/HTTPServer.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/router/Router.hpp>

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

        template <typename Handler>
        void get_heavy(const std::string &path, Handler handler)
        {
            add_route(http::verb::get, path, std::move(handler), vix::router::RouteOptions{.heavy = true});
        }

        template <typename Handler>
        void post_heavy(const std::string &path, Handler handler)
        {
            add_route(http::verb::post, path, std::move(handler), vix::router::RouteOptions{.heavy = true});
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

        using Next = std::function<void()>;
        using Middleware = std::function<void(vix::vhttp::Request &, vix::vhttp::ResponseWrapper &, Next)>;

        // Global middleware
        void use(Middleware mw);
        // Scoped middleware by path prefix (ex: "/api/")
        void use(std::string prefix, Middleware mw);

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

        // Handler detection helpers
        using RawRequestT = vix::vhttp::RawRequest;

        template <class H>
        static constexpr bool is_facade_handler_v =
            std::is_invocable_v<H &, vix::vhttp::Request &, vix::vhttp::ResponseWrapper &>;

        template <class H>
        static constexpr bool is_raw_handler_v =
            std::is_invocable_v<H &, const RawRequestT &, vix::vhttp::ResponseWrapper &>;

        template <typename Handler>
        void add_route(http::verb method, const std::string &path, Handler handler)
        {
            add_route(method, path, std::move(handler), vix::router::RouteOptions{});
        }

        template <typename Handler>
        void add_route(http::verb method,
                       const std::string &path,
                       Handler handler,
                       vix::router::RouteOptions opt)
        {
            auto &log = Logger::getInstance();
            if (!router_)
                log.throwError("Router is not initialized in App");

            static_assert(is_facade_handler_v<Handler> || is_raw_handler_v<Handler>,
                          "Invalid handler: expected (vix::vhttp::Request&, ResponseWrapper&) "
                          "or (const vix::vhttp::RawRequest&, ResponseWrapper&)");

            auto chain = collect_middlewares_for_(path);
            auto final = std::move(handler);

            // ✅ wrapped MUST be a ValidHandler for RequestHandler
            // => choose: (Request&, ResponseWrapper&)
            auto wrapped = [chain = std::move(chain), final = std::move(final)](
                               vix::vhttp::Request &req,
                               vix::vhttp::ResponseWrapper &res) mutable
            {
                std::function<void()> final_handler = [&]()
                {
                    if constexpr (is_facade_handler_v<decltype(final)>)
                    {
                        final(req, res);
                    }
                    else if constexpr (is_raw_handler_v<decltype(final)>)
                    {
                        final(req.raw(), res);
                    }
                    else
                    {
                        static_assert(is_facade_handler_v<decltype(final)> || is_raw_handler_v<decltype(final)>,
                                      "Unsupported handler signature.");
                    }
                };

                run_middleware_chain_(chain, 0, req, res, final_handler);
            };

            using Adapter = vix::vhttp::RequestHandler<decltype(wrapped)>;
            auto request_handler = std::make_shared<Adapter>(path, std::move(wrapped));

            router_->add_route(method, path, request_handler, opt);

            log.logf(Logger::Level::DEBUG,
                     "Route registered",
                     "method", static_cast<int>(method),
                     "path", path.c_str(),
                     "heavy", opt.heavy ? "true" : "false");
        }

        struct MiddlewareEntry
        {
            std::string prefix; // empty => global
            Middleware mw;
        };

        std::vector<MiddlewareEntry> middlewares_;
        bool match_middleware_prefix_(const std::string &prefix, const std::string &path) const;
        std::vector<Middleware> collect_middlewares_for_(const std::string &path) const;

        static inline void run_middleware_chain_(
            const std::vector<Middleware> &chain,
            std::size_t i,
            vix::vhttp::Request &req,
            vix::vhttp::ResponseWrapper &res,
            std::function<void()> final_handler)
        {
            if (i >= chain.size())
            {
                final_handler();
                return;
            }

            auto next = [&]()
            {
                run_middleware_chain_(chain, i + 1, req, res, std::move(final_handler));
            };

            chain[i](req, res, std::move(next));
        }
    };

} // namespace vix

#endif // VIX_APP_HPP
