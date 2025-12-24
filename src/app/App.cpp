#include <vix/app/App.hpp>

#include <vix/router/Router.hpp>
#include <vix/utils/Env.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>

#include <boost/beast/http.hpp>

#include <csignal>
#include <cctype>
#include <exception>
#include <string>
#include <thread>

namespace
{
    vix::utils::Logger::Level parse_log_level_from_env()
    {
        using Level = vix::utils::Logger::Level;

        const std::string raw = vix::utils::env_or("VIX_LOG_LEVEL", std::string{"warn"});

        std::string s;
        s.reserve(raw.size());
        for (char c : raw)
        {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        if (s == "trace")
            return Level::TRACE;
        if (s == "debug")
            return Level::DEBUG;
        if (s == "info")
            return Level::INFO;
        if (s == "warn" || s == "warning")
            return Level::WARN;
        if (s == "error")
            return Level::ERROR;
        if (s == "critical")
            return Level::CRITICAL;
        return Level::WARN;
    }

    std::size_t compute_executor_threads()
    {
        auto hc = std::thread::hardware_concurrency();
        if (hc == 0)
            hc = 4;
        return static_cast<std::size_t>(hc);
    }

    void register_bench_route(vix::router::Router &router)
    {
        namespace http = boost::beast::http;

        auto handler = [](vix::vhttp::Request &req, vix::vhttp::ResponseWrapper &res)
        {
            (void)req;
            res.ok().text("OK");
        };

        using HandlerT = vix::vhttp::RequestHandler<decltype(handler)>;
        auto h = std::make_shared<HandlerT>("/bench", handler);
        router.add_route(http::verb::get, "/bench", h);
    }

    static std::atomic<vix::App *> g_app_ptr{nullptr};

    static void handle_stop_signal(int /*signum*/)
    {
        if (auto *app = g_app_ptr.load(std::memory_order_relaxed))
        {
            app->request_stop_from_signal();
        }
    }

} // namespace

namespace vix
{
    using Logger = vix::utils::Logger;

    App::App()
        : config_(vix::config::Config::getInstance()),
          router_(nullptr),
          executor_(std::make_shared<vix::experimental::ThreadPoolExecutor>(
              compute_executor_threads(),
              compute_executor_threads(),
              /*defaultPriority*/ 1)),
          server_(config_, executor_)
    {
        auto &log = Logger::getInstance();

        log.setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        log.setLevel(parse_log_level_from_env());

        if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
        {
            log.setAsync(true);
            log.log(Logger::Level::DEBUG, "Logger initialized in ASYNC mode");
        }
        else
        {
            log.setAsync(false);
            log.log(Logger::Level::DEBUG, "Logger initialized in SYNC mode");
        }

        if (!vix::utils::env_bool("VIX_INTERNAL_LOGS", false))
        {
            // Le CLI/bannière gèrent l'affichage. On coupe les logs internes.
            log.setLevel(Logger::Level::CRITICAL);
        }

        Logger::Context ctx;
        ctx.module = "App";
        log.setContext(ctx);

        try
        {
            router_ = server_.getRouter();
            if (!router_)
            {
                log.throwError("Failed to get Router from HTTPServer");
            }

            register_bench_route(*router_);
        }
        catch (const std::exception &e)
        {
            log.throwError("Failed to initialize App: {}", e.what());
        }
    }

    App::App(std::shared_ptr<vix::executor::IExecutor> executor)
        : config_(vix::config::Config::getInstance()),
          router_(nullptr),
          executor_(std::move(executor)),
          server_(config_, executor_)
    {
        auto &log = Logger::getInstance();

        if (!executor_)
        {
            log.throwError("App: executor cannot be null");
        }

        log.setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        log.setLevel(parse_log_level_from_env());

        if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
        {
            log.setAsync(true);
            log.log(Logger::Level::DEBUG, "Logger initialized in ASYNC mode");
        }
        else
        {
            log.setAsync(false);
            log.log(Logger::Level::DEBUG, "Logger initialized in SYNC mode");
        }

        if (vix::utils::env_bool("VIX_INTERNAL_LOGS", false))
        {
            log.setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            log.setLevel(parse_log_level_from_env());
        }
        else
        {
            // Le CLI s’occupe déjà d'afficher.
            log.setLevel(Logger::Level::CRITICAL); // quasi muet
        }

        Logger::Context ctx;
        ctx.module = "App";
        log.setContext(ctx);

        try
        {
            router_ = server_.getRouter();
            if (!router_)
                log.throwError("Failed to get Router from HTTPServer");

            register_bench_route(*router_);
        }
        catch (const std::exception &e)
        {
            log.throwError("Failed to initialize App: {}", e.what());
        }
    }

    App::~App()
    {
        // Additive safety: if user used listen() and forgot close(), we avoid a running thread.
        close();
    }

    void App::request_stop_from_signal() noexcept
    {
        stop_requested_.store(true, std::memory_order_relaxed);
        stop_cv_.notify_one();

        // ask server to stop (idempotent)
        server_.stop_async();
    }

    void App::listen_port(int port, ListenPortCallback cb)
    {
        listen(port, [cb = std::move(cb)](const vix::utils::ServerReadyInfo &info)
               {
        if (cb) cb(info.port); });
    }

    void App::listen(int port, ListenCallback on_listen)
    {
        using clock = std::chrono::steady_clock;

        auto &log = Logger::getInstance();

        if (started_.exchange(true, std::memory_order_relaxed))
        {
            // si tu veux 0 logs, tu peux juste return;
            log.log(Logger::Level::WARN, "App::listen() called but server is already running");
            return;
        }

        const auto t0 = clock::now();

        stop_requested_.store(false, std::memory_order_relaxed);
        config_.setServerPort(port);

        g_app_ptr.store(this, std::memory_order_relaxed);
        std::signal(SIGINT, handle_stop_signal);
#ifdef SIGTERM
        std::signal(SIGTERM, handle_stop_signal);
#endif

        server_thread_ = std::thread([this]()
                                     { server_.run(); });

        log.log(Logger::Level::DEBUG, "[http] listen() called port={}", port);

        const auto t1 = clock::now();
        int ready_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        if (ready_ms < 1)
            ready_ms = 1;

        vix::utils::ServerReadyInfo info;
        info.app = "vix";
        info.version = "Vix.cpp v1.16.2";
        info.ready_ms = ready_ms;
        // Source de vérité: runtime/CLI
        info.mode = dev_mode_ ? "dev" : "run";

        // Optionnel: permettre override explicite
        if (const char *v = std::getenv("VIX_MODE"); v && *v)
            info.mode = vix::utils::RuntimeBanner::mode_from_env();

        info.scheme = "http";
        info.host = "localhost";
        info.port = port;
        info.base_path = "/";

        info.show_ws = false;

        if (auto *tp = dynamic_cast<vix::experimental::ThreadPoolExecutor *>(executor_.get()))
        {
            info.threads = tp->threads();
            info.max_threads = tp->max_threads();
        }
        else
        {
            const auto th = compute_executor_threads();
            info.threads = th;
            info.max_threads = th;
        }

        if (on_listen)
        {
            on_listen(info);
            return;
        }

        vix::utils::RuntimeBanner::emit_server_ready(info);
    }

    void App::wait()
    {
        std::unique_lock<std::mutex> lock(stop_mutex_);
        stop_cv_.wait(lock, [this]()
                      { return stop_requested_.load(std::memory_order_relaxed); });
    }

    void App::close()
    {
        if (!started_.load(std::memory_order_relaxed))
        {
            return;
        }

        stop_requested_.store(true, std::memory_order_relaxed);
        stop_cv_.notify_one();

        auto &log = Logger::getInstance();

        if (shutdown_cb_)
        {
            try
            {
                shutdown_cb_();
            }
            catch (const std::exception &e)
            {
                log.log(Logger::Level::ERROR, "Shutdown callback threw: {}", e.what());
            }
            catch (...)
            {
                log.log(Logger::Level::ERROR, "Shutdown callback threw unknown exception");
            }
        }

        server_.stop_async();
        server_.stop_blocking();

        if (server_thread_.joinable())
        {
            server_thread_.join();
        }

        g_app_ptr.store(nullptr, std::memory_order_relaxed);
        started_.store(false, std::memory_order_relaxed);

        log.log(Logger::Level::DEBUG, "Application shutdown complete");
    }

    void App::run(int port)
    {
        // app.run(8080) still blocks until SIGINT/SIGTERM, then shuts down gracefully.
        listen(port);
        wait();
        close();
    }

    void App::use(Middleware mw)
    {
        middlewares_.push_back(MiddlewareEntry{"", std::move(mw)});
    }

    void App::use(std::string prefix, Middleware mw)
    {
        middlewares_.push_back(MiddlewareEntry{std::move(prefix), std::move(mw)});
    }

    bool App::match_middleware_prefix_(const std::string &prefix, const std::string &path) const
    {
        if (prefix.empty())
            return true;
        if (path.size() < prefix.size())
            return false;
        return path.compare(0, prefix.size(), prefix) == 0;
    }

    std::vector<App::Middleware> App::collect_middlewares_for_(const std::string &path) const
    {
        std::vector<Middleware> out;
        out.reserve(middlewares_.size());

        for (const auto &e : middlewares_)
        {
            if (match_middleware_prefix_(e.prefix, path))
                out.push_back(e.mw);
        }
        return out;
    }

} // namespace vix
