/**
 * @file App.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_HTTP_APP_HPP
#define VIX_HTTP_APP_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
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
  class Router;
}

namespace vix
{
  namespace http = boost::beast::http;
  using Logger = vix::utils::Logger;

  /** @brief Return the global Vix logger instance. */
  inline Logger &log()
  {
    return Logger::getInstance();
  }

  /** @brief HTTP application wrapper that owns the router, server, and execution context. */
  class App
  {
  public:
    /** @brief Callback invoked when the app is shutting down. */
    using ShutdownCallback = std::function<void()>;

    /** @brief Callback invoked when the server is ready and listening. */
    using ListenCallback = std::function<void(const vix::utils::ServerReadyInfo &)>;

    /** @brief Callback invoked with the bound listening port. */
    using ListenPortCallback = std::function<void(int)>;

    /** @brief Create an app using the default executor. */
    App();

    /** @brief Create an app using a custom executor shared by the HTTP server. */
    explicit App(std::shared_ptr<vix::executor::IExecutor> executor);

    /** @brief Destroy the app and stop the server if still running. */
    ~App();

    App(const App &) = delete;
    App &operator=(const App &) = delete;
    App(App &&) = delete;
    App &operator=(App &&) = delete;

    /** @brief Run the HTTP server and block the current thread until it stops. */
    void run(int port = 8080);

    /** @brief Start listening on a port in a background thread and optionally receive a ready callback. */
    void listen(int port = 8080, ListenCallback on_listen = {});

    /** @brief Block until the server has fully stopped. */
    void wait();

    /** @brief Request the server to stop and wake any waiting thread. */
    void close();

    /** @brief Set a callback executed once during shutdown. */
    void set_shutdown_callback(ShutdownCallback cb)
    {
      shutdown_cb_ = std::move(cb);
    }

    /** @brief Register a GET handler for the given path. */
    template <typename Handler>
    void get(const std::string &path, Handler handler)
    {
      add_route(http::verb::get, path, std::move(handler));
    }

    /** @brief Register a POST handler for the given path. */
    template <typename Handler>
    void post(const std::string &path, Handler handler)
    {
      add_route(http::verb::post, path, std::move(handler));
    }

    /** @brief Register a PUT handler for the given path. */
    template <typename Handler>
    void put(const std::string &path, Handler handler)
    {
      add_route(http::verb::put, path, std::move(handler));
    }

    /** @brief Register a PATCH handler for the given path. */
    template <typename Handler>
    void patch(const std::string &path, Handler handler)
    {
      add_route(http::verb::patch, path, std::move(handler));
    }

    /** @brief Register a DELETE handler for the given path. */
    template <typename Handler>
    void del(const std::string &path, Handler handler)
    {
      add_route(http::verb::delete_, path, std::move(handler));
    }

    /** @brief Register a HEAD handler for the given path. */
    template <typename Handler>
    void head(const std::string &path, Handler handler)
    {
      add_route(http::verb::head, path, std::move(handler));
    }

    /** @brief Register an OPTIONS handler for the given path. */
    template <typename Handler>
    void options(const std::string &path, Handler handler)
    {
      add_route(http::verb::options, path, std::move(handler));
    }

    /** @brief Register a GET handler marked as heavy work (scheduler can treat it differently). */
    template <typename Handler>
    void get_heavy(const std::string &path, Handler handler)
    {
      add_route(http::verb::get, path, std::move(handler), vix::router::RouteOptions{.heavy = true});
    }

    /** @brief Register a POST handler marked as heavy work (scheduler can treat it differently). */
    template <typename Handler>
    void post_heavy(const std::string &path, Handler handler)
    {
      add_route(http::verb::post, path, std::move(handler), vix::router::RouteOptions{.heavy = true});
    }

    /** @brief Access the global configuration used by the app. */
    vix::config::Config &config() noexcept { return config_; }

    /** @brief Get the router instance used to register routes (may be null before init). */
    std::shared_ptr<vix::router::Router> router() const noexcept { return router_; }

    /** @brief Access the underlying HTTP server instance. */
    vix::server::HTTPServer &server() noexcept { return server_; }

    /** @brief Access the executor used by the server. */
    vix::executor::IExecutor &executor() noexcept { return *executor_; }

    /** @brief Return true if the server has started. */
    bool is_running() const noexcept { return started_.load(std::memory_order_relaxed); }

    /** @brief Request stop in a signal-safe way (intended for SIGINT/SIGTERM handlers). */
    void request_stop_from_signal() noexcept;

    /** @brief Start listening on a port and optionally receive the resolved port value. */
    void listen_port(int port, ListenPortCallback cb = {});

    /** @brief Enable or disable development mode for the app. */
    void setDevMode(bool v) { dev_mode_ = v; }

    /** @brief Return whether development mode is enabled. */
    bool isDevMode() const { return dev_mode_; }

    /** @brief Signature for the static assets handler used by static_dir(). */
    using StaticHandler =
        std::function<bool(App &, const std::filesystem::path &root,
                           const std::string &mount,
                           const std::string &index_file,
                           bool add_cache_control,
                           const std::string &cache_control,
                           bool fallthrough)>;

    /** @brief Set the global static assets handler used by all App instances. */
    static void set_static_handler(StaticHandler fn);

    /** @brief Serve a directory of static files under a mount path with optional cache control. */
    void static_dir(std::filesystem::path root,
                    std::string mount = "/",
                    std::string index_file = "index.html",
                    bool add_cache_control = true,
                    std::string cache_control = "public, max-age=3600",
                    bool fallthrough = true);

    /** @brief Function pointer type used to initialize optional modules once. */
    using ModuleInitFn = void (*)();

    /** @brief Set a global module initializer called by the runtime. */
    static void set_module_init(ModuleInitFn fn);

    /** @brief Continuation invoked to call the next middleware in the chain. */
    using Next = std::function<void()>;

    /** @brief Middleware signature used by use(), protect(), and groups. */
    using Middleware = std::function<void(vix::vhttp::Request &, vix::vhttp::ResponseWrapper &, Next)>;

    /** @brief Route group helper that prefixes paths and shares middleware registration. */
    class Group
    {
    public:
      /** @brief Create a group with a normalized prefix. */
      Group(App &app, std::string prefix)
          : app_(&app), prefix_(normalize_prefix(std::move(prefix)))
      {
      }

      /** @brief Create a nested group under the current prefix. */
      template <class Fn>
      void group(std::string sub, Fn fn)
      {
        Group g(*app_, join(prefix_, std::move(sub)));
        fn(g);
      }

      /** @brief Attach a middleware to all routes under this group prefix. */
      Group &use(Middleware mw)
      {
        app_->use(prefix_, std::move(mw));
        return *this;
      }

      /** @brief Attach a middleware to a sub-prefix under this group. */
      Group &protect(std::string sub_prefix, Middleware mw)
      {
        app_->protect(join(prefix_, std::move(sub_prefix)), std::move(mw));
        return *this;
      }

      /** @brief Attach a middleware that runs only when the request path matches exactly. */
      Group &protect_exact(std::string sub_path, Middleware mw)
      {
        app_->protect_exact(join(prefix_, std::move(sub_path)), std::move(mw));
        return *this;
      }

      /** @brief Register a GET handler under this group prefix. */
      template <typename Handler>
      void get(const std::string &path, Handler handler)
      {
        app_->get(join(prefix_, path), std::move(handler));
      }

      /** @brief Register a POST handler under this group prefix. */
      template <typename Handler>
      void post(const std::string &path, Handler handler)
      {
        app_->post(join(prefix_, path), std::move(handler));
      }

      /** @brief Register a PUT handler under this group prefix. */
      template <typename Handler>
      void put(const std::string &path, Handler handler)
      {
        app_->put(join(prefix_, path), std::move(handler));
      }

      /** @brief Register a PATCH handler under this group prefix. */
      template <typename Handler>
      void patch(const std::string &path, Handler handler)
      {
        app_->patch(join(prefix_, path), std::move(handler));
      }

      /** @brief Register a DELETE handler under this group prefix. */
      template <typename Handler>
      void del(const std::string &path, Handler handler)
      {
        app_->del(join(prefix_, path), std::move(handler));
      }

    private:
      static std::string normalize_prefix(std::string p)
      {
        if (p.empty())
          return "";

        if (p.front() != '/')
          p.insert(p.begin(), '/');

        while (p.size() > 1 && p.back() == '/')
          p.pop_back();

        return p;
      }

      static std::string join(std::string base, std::string sub)
      {
        base = normalize_prefix(std::move(base));

        if (sub.empty())
          return base;

        if (sub.front() != '/')
          sub.insert(sub.begin(), '/');

        while (sub.size() > 1 && sub.back() == '/')
          sub.pop_back();

        if (base.empty())
          return sub;

        return base + sub;
      }

    private:
      App *app_{nullptr};
      std::string prefix_;
    };

    /** @brief Create a temporary group and call the provided function to register routes. */
    template <class Fn>
    void group(std::string prefix, Fn fn)
    {
      Group g(*this, std::move(prefix));
      fn(g);
    }

    /** @brief Create a group object for incremental route registration. */
    Group group(std::string prefix)
    {
      return Group(*this, std::move(prefix));
    }

    /** @brief Attach a global middleware that applies to all routes. */
    void use(Middleware mw);

    /** @brief Attach a middleware that applies to all routes under the given prefix. */
    void use(std::string prefix, Middleware mw);

    /** @brief Alias for use(prefix, mw) with prefix normalization. */
    void protect(std::string prefix, Middleware mw)
    {
      use(normalize_prefix(std::move(prefix)), std::move(mw));
    }

    /** @brief Attach a middleware that runs only when the request path equals the given path. */
    void protect_exact(std::string path, Middleware mw)
    {
      std::string match = normalize_prefix(std::move(path));
      use(match, [mw = std::move(mw), match](vix::vhttp::Request &req,
                                             vix::vhttp::ResponseWrapper &res,
                                             Next next) mutable
          {
        if (req.path() == match)
          mw(req, res, std::move(next));
        else
          next(); });
    }

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
      if (!router_)
        log().throwError("Router is not initialized in App");

      static_assert(is_facade_handler_v<Handler> || is_raw_handler_v<Handler>,
                    "Invalid handler: expected (vix::vhttp::Request&, ResponseWrapper&) "
                    "or (const vix::vhttp::RawRequest&, ResponseWrapper&)");

      auto chain = collect_middlewares_for_(path);
      auto final = std::move(handler);

      auto wrapped = [chain = std::move(chain), final = std::move(final)](
                         vix::vhttp::Request &req,
                         vix::vhttp::ResponseWrapper &res) mutable
      {
        std::function<void()> final_handler = [&]()
        {
          auto should_auto_send = [&]() -> bool
          {
            return res.res.body().empty() &&
                   res.res.find(http::field::content_length) == res.res.end();
          };

          if constexpr (is_facade_handler_v<decltype(final)>)
          {
            using Ret = std::invoke_result_t<decltype(final), vix::vhttp::Request &, vix::vhttp::ResponseWrapper &>;

            if constexpr (std::is_void_v<Ret>)
            {
              final(req, res);
            }
            else
            {
              auto out = final(req, res);
              if (should_auto_send())
                res.send(out);
            }
          }
          else if constexpr (is_raw_handler_v<decltype(final)>)
          {
            using Ret = std::invoke_result_t<decltype(final), const RawRequestT &, vix::vhttp::ResponseWrapper &>;

            if constexpr (std::is_void_v<Ret>)
            {
              final(req.raw(), res);
            }
            else
            {
              auto out = final(req.raw(), res);
              if (should_auto_send())
                res.send(out);
            }
          }
        };

        run_middleware_chain_(chain, 0, req, res, final_handler);
      };

      using Adapter = vix::vhttp::RequestHandler<decltype(wrapped)>;
      auto request_handler = std::make_shared<Adapter>(path, std::move(wrapped));

      router_->add_route(method, path, request_handler, opt);

      if (method != http::verb::options)
      {
        ensure_options_route_for_path_(path);
      }
    }

    struct MiddlewareEntry
    {
      std::string prefix;
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

    static std::string normalize_prefix(std::string p)
    {
      if (p.empty())
        return "";
      if (p.front() != '/')
        p.insert(p.begin(), '/');
      while (p.size() > 1 && p.back() == '/')
        p.pop_back();
      return p;
    }

    void ensure_options_route_for_path_(const std::string &path)
    {
      if (!router_)
        return;

      if (router_->has_route(http::verb::options, path))
        return;

      auto chain = collect_middlewares_for_(path);

      auto wrapped = [chain = std::move(chain)](
                         vix::vhttp::Request &req,
                         vix::vhttp::ResponseWrapper &res) mutable
      {
        std::function<void()> final_handler = [&]()
        {
          if (res.res.result() == http::status::unknown && res.res.body().empty())
          {
            res.status(204).send();
          }
          else if (res.res.result() == http::status::unknown)
          {
            res.send();
          }
        };

        run_middleware_chain_(chain, 0, req, res, final_handler);
      };

      using Adapter = vix::vhttp::RequestHandler<decltype(wrapped)>;
      auto request_handler = std::make_shared<Adapter>(path, std::move(wrapped));

      router_->add_route(http::verb::options, path, request_handler, vix::router::RouteOptions{});
    }
  };

} // namespace vix

#endif // VIX_HTTP_APP_HPP
