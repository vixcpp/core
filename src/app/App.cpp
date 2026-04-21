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

#include <vix/openapi/register_docs.hpp>
#include <vix/router/Router.hpp>
#include <vix/utils/Env.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/ScopeGuard.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>

#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <utility>

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
        [](vix::http::Request &req,
           vix::http::ResponseWrapper &res,
           vix::App::Next next)
        {
          static const bool kAccessLogs = vix::utils::env_bool("VIX_ACCESS_LOGS", true);
          if (!kAccessLogs)
          {
            next();
            return;
          }

          if (!log().enabled(vix::utils::Logger::Level::Debug))
          {
            next();
            return;
          }

          using clock = std::chrono::steady_clock;
          const auto t0 = clock::now();
          const auto rid = g_rid_seq.fetch_add(1, std::memory_order_relaxed) + 1;

          next();

          const auto t1 = clock::now();
          const auto ms =
              std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
          const int status = vix::http::is_valid_status(res.res.status())
                                 ? res.res.status()
                                 : vix::http::OK;

          log().logf(
              vix::utils::Logger::Level::Debug,
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
      return Level::Trace;
    if (s == "debug")
      return Level::Debug;
    if (s == "info")
      return Level::Info;
    if (s == "warn" || s == "warning")
      return Level::Warn;
    if (s == "error")
      return Level::Error;
    if (s == "critical")
      return Level::Critical;

    return Level::Warn;
  }

  std::size_t compute_executor_threads()
  {
    auto hc = std::thread::hardware_concurrency();
    if (hc == 0)
      hc = 4;

    return static_cast<std::size_t>(hc);
  }

  std::shared_ptr<vix::executor::RuntimeExecutor> make_default_executor()
  {
    const auto threads = static_cast<std::uint32_t>(compute_executor_threads());

    auto exec = std::make_shared<vix::executor::RuntimeExecutor>(threads);
    exec->start();
    return exec;
  }

  void register_bench_route(vix::router::Router &router)
  {
    auto handler = [](vix::http::Request &req, vix::http::ResponseWrapper &res)
    {
      (void)req;

#ifdef VIX_BENCH_MODE
      res.ok().text("OK");
#else
      res.ok().text("OK");
#endif
    };

    using HandlerT = vix::http::RequestHandler<decltype(handler)>;
    auto h = std::make_shared<HandlerT>("/bench", handler);
    router.add_route("GET", "/bench", h);
  }

  static std::atomic<bool> g_signal_stop_requested{false};

  static void handle_stop_signal(int /*signum*/)
  {
    g_signal_stop_requested.store(true, std::memory_order_relaxed);
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

  void App::set_module_init(ModuleInitFn fn)
  {
    module_init_ref() = fn;
  }

  static void init_modules_once()
  {
    std::call_once(g_module_init_once, []
                   {
                     if (auto fn = module_init_ref())
                     {
                       fn();
                     } });
  }

  App::App()
      : config_(),
        router_(nullptr),
        executor_(make_default_executor()),
        server_(config_, executor_)
  {
    log().setLevelFromEnv("VIX_LOG_LEVEL");
    log().setFormatFromEnv("VIX_LOG_FORMAT");

    if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
      log().setAsync(true);
    else
      log().setAsync(false);

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

      setup_not_found_handler_();

      if (vix::utils::env_bool("VIX_DOCS", true))
      {
        vix::openapi::register_openapi_and_docs(*router_, "Vix API", "2.5.0");
      }

      install_access_logs(*this);
      register_bench_route(*router_);
    }
    catch (const std::exception &e)
    {
      log().throwError("Failed to initialize App: {}", e.what());
    }
  }

  App::App(std::shared_ptr<vix::executor::RuntimeExecutor> executor)
      : config_(),
        router_(nullptr),
        executor_(std::move(executor)),
        server_(config_, executor_)
  {
    if (!executor_)
    {
      log().throwError("App: executor cannot be null");
    }

    executor_->start();

    log().setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    log().setLevel(parse_log_level_from_env());

    if (vix::utils::env_bool("VIX_LOG_ASYNC", true))
      log().setAsync(true);
    else
      log().setAsync(false);

    if (vix::utils::env_bool("VIX_INTERNAL_LOGS", false))
    {
      log().setPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
      log().setLevel(parse_log_level_from_env());
    }
    else
    {
      log().setLevel(Logger::Level::Critical);
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

      setup_not_found_handler_();

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

  void App::setup_not_found_handler_()
  {
    router_->setNotFoundHandler(
        [this](const vix::http::Request &req,
               vix::http::Response &res) -> vix::async::core::task<void>
        {
          vix::http::ResponseWrapper resw(res, template_view_.get());

          // Static fallback
          if (this->try_static_file_(const_cast<vix::http::Request &>(req), resw))
          {
            co_return;
          }

          // Default 404
          res.set_status(vix::http::NOT_FOUND);

          nlohmann::json j{
              {"error", "Route not found"},
              {"hint", "Check path, method, or API version"},
              {"method", req.method()},
              {"path", req.target()}};

          vix::http::Response::json_response(res, j, vix::http::NOT_FOUND);
          res.set_header("Connection", "close");
          res.set_should_close(true);

          co_return;
        });
  }

  App &App::templates(const std::string &directory)
  {
    auto loader =
        std::make_shared<vix::template_::FileSystemLoader>(directory);

    template_engine_ =
        std::make_shared<vix::template_::Engine>(std::move(loader));

    template_view_ =
        std::make_unique<vix::view::TemplateView>(template_engine_);

    templates_directory_ = directory;

    return *this;
  }

  bool App::has_views() const noexcept
  {
    return static_cast<bool>(template_engine_) &&
           static_cast<bool>(template_view_);
  }

  vix::view::TemplateView &App::views()
  {
    if (!template_view_)
    {
      throw std::runtime_error(
          "App::views() called before templates() configuration");
    }

    return *template_view_;
  }

  const vix::view::TemplateView &App::views() const
  {
    if (!template_view_)
    {
      throw std::runtime_error(
          "App::views() called before templates() configuration");
    }

    return *template_view_;
  }

  App::~App()
  {
    if (listen_called_.load(std::memory_order_relaxed) &&
        !wait_called_.load(std::memory_order_relaxed))
    {
      log().log(Logger::Level::Warn, "listen() without wait()");
    }

    close();
  }

  void App::request_stop_from_signal() noexcept
  {
    stop_requested_.store(true, std::memory_order_relaxed);
  }

  void App::listen_port(int port, ListenPortCallback cb)
  {
    listen(port, [this, cb = std::move(cb)]()
           {
             const int bound = server_.bound_port();
             if (cb)
             {
               cb(bound);
             } });
  }

  void App::listen(int port, ListenCallback on_listen)
  {
    using clock = std::chrono::steady_clock;

    listen_called_.store(true, std::memory_order_relaxed);

    if (started_.exchange(true, std::memory_order_relaxed))
    {
      log().log(Logger::Level::Warn, "App::listen() called but server is already running");
      return;
    }

    const auto t0 = clock::now();

    stop_requested_.store(false, std::memory_order_relaxed);
    config_.setServerPort(port);

    std::signal(SIGINT, handle_stop_signal);
#ifdef SIGTERM
    std::signal(SIGTERM, handle_stop_signal);
#endif

    server_thread_ = std::thread([this]()
                                 { server_.run(); });

    int bound = 0;
    for (int i = 0; i < 200; ++i)
    {
      bound = server_.bound_port();
      if (bound != 0)
      {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto t1 = clock::now();
    int ready_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    if (ready_ms < 1)
    {
      ready_ms = 1;
    }

    vix::utils::ServerReadyInfo info;
    info.app = "vix.cpp";
    info.version = "v2.5.0";
    info.ready_ms = ready_ms;
    info.mode = dev_mode_ ? "dev" : "run";

    if (const char *v = vix::utils::vix_getenv("VIX_MODE"); v && *v)
    {
      info.mode = vix::utils::RuntimeBanner::mode_from_env();
    }

    info.scheme = "http";
    info.host = "localhost";
    info.port = (bound != 0) ? bound : config_.getServerPort();
    info.base_path = "/";
    info.show_ws = false;

    const auto th = compute_executor_threads();
    info.threads = th;
    info.max_threads = th;

    {
      std::lock_guard<std::mutex> lock(ready_info_mutex_);
      last_ready_info_ = info;
      has_ready_info_.store(true, std::memory_order_relaxed);
    }

    if (on_listen)
    {
      on_listen();
    }
    else
    {
      vix::utils::RuntimeBanner::emit_server_ready(info);
    }
  }

  void App::wait()
  {
    wait_called_.store(true, std::memory_order_relaxed);

    for (;;)
    {
      if (g_signal_stop_requested.exchange(false, std::memory_order_relaxed))
      {
        stop_requested_.store(true, std::memory_order_relaxed);
        stop_cv_.notify_one();
        break;
      }

      if (stop_requested_.load(std::memory_order_relaxed))
      {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  void App::close()
  {
    bool expected = false;
    if (!closed_.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
      return;
    }

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
      catch (...)
      {
      }
    }

    server_.stop_async();
    server_.stop_blocking();

    if (server_thread_.joinable())
    {
      server_thread_.join();
    }

    started_.store(false, std::memory_order_relaxed);
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
    middlewares_.push_back(
        MiddlewareEntry{normalize_prefix(std::move(prefix)), std::move(mw)});
  }

  bool App::match_middleware_prefix_(const std::string &prefix,
                                     const std::string &path) const
  {
    if (prefix.empty() || prefix == "/")
    {
      return true;
    }

    if (path.size() < prefix.size())
    {
      return false;
    }

    if (path.compare(0, prefix.size(), prefix) != 0)
    {
      return false;
    }

    if (path.size() == prefix.size())
    {
      return true;
    }

    return path[prefix.size()] == '/';
  }

  std::vector<App::Middleware> App::collect_middlewares_for_(const std::string &path) const
  {
    std::vector<Middleware> out;
    out.reserve(middlewares_.size());

    for (const auto &e : middlewares_)
    {
      if (e.prefix.empty())
      {
        out.push_back(e.mw);
      }
    }

    for (const auto &e : middlewares_)
    {
      if (!e.prefix.empty() && match_middleware_prefix_(e.prefix, path))
      {
        out.push_back(e.mw);
      }
    }

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
    static_mounts_.push_back(StaticMount{
        std::move(root),
        normalize_prefix(std::move(mount)),
        std::move(index_file),
        add_cache_control,
        std::move(cache_control),
        fallthrough});
  }

  bool App::serve_static_from_mount_(const StaticMount &m,
                                     vix::http::Request &req,
                                     vix::http::ResponseWrapper &res)
  {
    const auto &method = req.method();
    if (!(method == "GET" || method == "HEAD"))
      return false;

    const std::string path = req.path();
    if (!match_middleware_prefix_(m.mount, path))
      return false;

    std::string rel;

    if (m.mount == "/")
    {
      rel = path;
    }
    else
    {
      rel = path.substr(m.mount.size());
    }

    if (!rel.empty() && rel.front() == '/')
      rel.erase(rel.begin());

    if (rel.empty())
      rel = m.index_file;

    if (rel.find("..") != std::string::npos)
    {
      res.status(400).text("Bad path");
      return true;
    }

    std::filesystem::path full = m.root / rel;

    std::error_code ec;
    if (std::filesystem::is_directory(full, ec))
      full /= m.index_file;

    if (!std::filesystem::exists(full, ec) || !std::filesystem::is_regular_file(full, ec))
    {
      if (m.fallthrough)
        return false;

      res.status(404).text("Not Found");
      return true;
    }

    if (m.add_cache_control)
      res.header("Cache-Control", m.cache_control);

    if (method == "HEAD")
    {
      res.file(full);
      res.res.set_body("");
      return true;
    }

    res.file(full);
    return true;
  }

  bool App::try_static_file_(vix::http::Request &req, vix::http::ResponseWrapper &res)
  {
    for (const auto &m : static_mounts_)
    {
      if (serve_static_from_mount_(m, req, res))
        return true;
    }

    return false;
  }

} // namespace vix
