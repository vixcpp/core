/**
 *
 *  @file App.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#include <vix/app/App.hpp>

#include <vix/router/Router.hpp>
#include <vix/utils/Env.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>
#include <vix/utils/ScopeGuard.hpp>
#include <vix/openapi/register_docs.hpp>

#include <boost/beast/http.hpp>

#include <csignal>
#include <cctype>
#include <exception>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

namespace
{
  static std::atomic<std::uint64_t> g_rid_seq{0};
  static std::once_flag g_module_init_once;

  inline vix::utils::Logger &log()
  {
    return vix::utils::Logger::getInstance();
  }

  static void install_access_logs(vix::App &app)
  {
    app.use(
        [](vix::vhttp::Request &req,
           vix::vhttp::ResponseWrapper &res,
           vix::App::Next next)
        {
          static const bool kAccessLogs = vix::utils::env_bool("VIX_ACCESS_LOGS", true);
          if (!kAccessLogs)
          {
            next();
            return;
          }

          if (!log().enabled(vix::utils::Logger::Level::DEBUG))
          {
            next();
            return;
          }

          using clock = std::chrono::steady_clock;
          const auto t0 = clock::now();
          const auto rid = g_rid_seq.fetch_add(1, std::memory_order_relaxed) + 1;

          next();

          const auto t1 = clock::now();
          const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
          const unsigned int status = res.res.result_int();

          log().logf(
              vix::utils::Logger::Level::DEBUG,
              "request_done",
              "rid", static_cast<unsigned long long>(rid),
              "method", req.method(),
              "path", req.path(),
              "status", status,
              "duration_ms", static_cast<long long>(ms));
        });
  }

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

  static vix::App::ModuleInitFn &module_init_ref()
  {
    static vix::App::ModuleInitFn fn = nullptr;
    return fn;
  }

  void vix::App::set_module_init(ModuleInitFn fn)
  {
    module_init_ref() = fn;
  }

  static void init_modules_once()
  {
    std::call_once(g_module_init_once, []
                   {
    if (auto fn = module_init_ref())
      fn(); });
  }

  App::App()
      : config_(vix::config::Config::getInstance()),
        router_(nullptr),
        executor_(std::make_shared<vix::experimental::ThreadPoolExecutor>(
            compute_executor_threads(),
            compute_executor_threads(),
            /*defaultPriority*/ 1)),
        server_(config_, executor_)
  {
    log().setLevelFromEnv("VIX_LOG_LEVEL");
    log().setFormatFromEnv("VIX_LOG_FORMAT");

    if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
    {
      log().setAsync(true);
    }
    else
    {
      log().setAsync(false);
    }

    Logger::Context ctx;
    ctx.module = "App";
    log().setContext(ctx);

    try
    {
      init_modules_once();
      router_ = server_.getRouter();
      if (!router_)
      {
        log().throwError("Failed to get Router from HTTPServer");
      }

      // Auto docs
      // Disable with: VIX_DOCS=0
      if (vix::utils::env_bool("VIX_DOCS", true))
      {
        vix::openapi::register_openapi_and_docs(*router_, "Vix API", "0.0.0");
      }

      install_access_logs(*this);
      register_bench_route(*router_);
    }
    catch (const std::exception &e)
    {
      log().throwError("Failed to initialize App: {}", e.what());
    }
  }

  App::App(std::shared_ptr<vix::executor::IExecutor> executor)
      : config_(vix::config::Config::getInstance()),
        router_(nullptr),
        executor_(std::move(executor)),
        server_(config_, executor_)
  {
    if (!executor_)
    {
      log().throwError("App: executor cannot be null");
    }

    log().setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    log().setLevel(parse_log_level_from_env());

    if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
    {
      log().setAsync(true);
    }
    else
    {
      log().setAsync(false);
    }

    if (vix::utils::env_bool("VIX_INTERNAL_LOGS", false))
    {
      log().setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
      log().setLevel(parse_log_level_from_env());
    }
    else
    {
      log().setLevel(Logger::Level::CRITICAL);
    }

    Logger::Context ctx;
    ctx.module = "App";
    log().setContext(ctx);

    try
    {
      init_modules_once();
      router_ = server_.getRouter();
      if (!router_)
        log().throwError("Failed to get Router from HTTPServer");

      // Auto docs
      // Disable with: VIX_DOCS=0
      if (vix::utils::env_bool("VIX_DOCS", true))
      {
        vix::openapi::register_openapi_and_docs(*router_, "Vix API", "0.0.0");
      }

      install_access_logs(*this);
      register_bench_route(*router_);
    }
    catch (const std::exception &e)
    {
      log().throwError("Failed to initialize App: {}", e.what());
    }
  }

  App::~App()
  {
    if (listen_called_.load(std::memory_order_relaxed) &&
        !wait_called_.load(std::memory_order_relaxed))
    {
      log().log(Logger::Level::WARN, "listen() without wait()");
    }

    close();
  }

  void App::request_stop_from_signal() noexcept
  {
    stop_requested_.store(true, std::memory_order_relaxed);
    stop_cv_.notify_one();

    server_.stop_async();
  }

  void App::listen_port(int port, ListenPortCallback cb)
  {
    listen(port, [this, cb = std::move(cb)]()
           {
           const int bound = server_.bound_port();
           if (cb)
             cb(bound); });
  }

  void App::listen(int port, ListenCallback on_listen)
  {
    using clock = std::chrono::steady_clock;
    listen_called_.store(true, std::memory_order_relaxed);

    if (started_.exchange(true, std::memory_order_relaxed))
    {
      log().log(Logger::Level::WARN, "App::listen() called but server is already running");
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

    int bound = 0;
    for (int i = 0; i < 200; ++i) // ~200ms max
    {
      bound = server_.bound_port();
      if (bound != 0)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto t1 = clock::now();
    int ready_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    if (ready_ms < 1)
      ready_ms = 1;

    vix::utils::ServerReadyInfo info;
    info.app = "vix.cpp";
    info.version = "v1.31.0";
    info.ready_ms = ready_ms;
    info.mode = dev_mode_ ? "dev" : "run";

    if (const char *v = std::getenv("VIX_MODE"); v && *v)
      info.mode = vix::utils::RuntimeBanner::mode_from_env();

    info.scheme = "http";
    info.host = "localhost";
    info.port = (bound != 0) ? bound : config_.getServerPort(); // fallback
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

    {
      std::lock_guard<std::mutex> lock(ready_info_mutex_);
      last_ready_info_ = info;
      has_ready_info_.store(true, std::memory_order_relaxed);
    }

    if (on_listen)
      on_listen();
    else
      vix::utils::RuntimeBanner::emit_server_ready(info);
  }

  void App::wait()
  {
    wait_called_.store(true, std::memory_order_relaxed);
    std::unique_lock<std::mutex> lock(stop_mutex_);
    stop_cv_.wait(
        lock, [this]()
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

    if (shutdown_cb_)
    {
      try
      {
        shutdown_cb_();
      }
      catch (const std::exception &e)
      {
        log().log(Logger::Level::ERROR, "Shutdown callback threw: {}", e.what());
      }
      catch (...)
      {
        log().log(Logger::Level::ERROR, "Shutdown callback threw unknown exception");
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

    log().log(Logger::Level::DEBUG, "Application shutdown complete");
  }

  void App::run(int port)
  {
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
    middlewares_.push_back(MiddlewareEntry{normalize_prefix(std::move(prefix)), std::move(mw)});
  }

  bool App::match_middleware_prefix_(
      const std::string &prefix,
      const std::string &path) const
  {
    if (prefix.empty())
      return true;

    if (path.size() < prefix.size())
      return false;

    if (path.compare(0, prefix.size(), prefix) != 0)
      return false;

    if (path.size() == prefix.size())
      return true;

    return path[prefix.size()] == '/';
  }

  std::vector<App::Middleware> App::collect_middlewares_for_(const std::string &path) const
  {
    std::vector<Middleware> out;
    out.reserve(middlewares_.size());

    for (const auto &e : middlewares_)
      if (e.prefix.empty())
        out.push_back(e.mw);

    for (const auto &e : middlewares_)
      if (!e.prefix.empty() && match_middleware_prefix_(e.prefix, path))
        out.push_back(e.mw);

    return out;
  }

  static App::StaticHandler &static_handler_ref()
  {
    static App::StaticHandler h{};
    return h;
  }

  void App::set_static_handler(StaticHandler fn)
  {
    static_handler_ref() = std::move(fn);
  }

  void App::static_dir(std::filesystem::path root,
                       std::string mount,
                       std::string index_file,
                       bool add_cache_control,
                       std::string cache_control,
                       bool fallthrough)
  {
    auto &h = static_handler_ref();
    if (!h)
    {
      // middleware module not linked → message explicite
      log().throwError("App::static_dir() requires vix::middleware module (static handler not registered)");
      return;
    }

    const bool ok = h(*this, root, mount, index_file,
                      add_cache_control, cache_control, fallthrough);

    if (!ok)
    {
      log().throwError("App::static_dir() failed (static handler returned false)");
    }
  }

} // namespace vix
