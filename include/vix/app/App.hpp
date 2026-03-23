/**
 * @file App.hpp
 * @brief Main Vix application object used to configure routes, middleware and server lifecycle.
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
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/beast/http.hpp>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/RequestHandler.hpp>
#include <vix/router/Router.hpp>
#include <vix/server/HTTPServer.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>

namespace vix::router
{
  class Router;
}

namespace vix
{
  namespace http = boost::beast::http;

  using Logger = vix::utils::Logger;

  /**
   * @brief Returns the global Vix logger instance.
   *
   * @return Logger& Global logger singleton.
   */
  inline Logger &log()
  {
    return Logger::getInstance();
  }

  /**
   * @brief Main application entry point for building a Vix HTTP server.
   *
   * This class owns the router, runtime executor and HTTP server. It provides
   * route registration helpers, middleware support, static file mounting and
   * lifecycle methods such as listen(), run(), wait() and close().
   */
  class App
  {
  public:
    /**
     * @brief Callback executed when the application is shutting down.
     */
    using ShutdownCallback = std::function<void()>;

    /**
     * @brief Callback executed when the server starts listening.
     */
    using ListenCallback = std::function<void()>;

    /**
     * @brief Callback executed when the server starts listening on a given port.
     */
    using ListenPortCallback = std::function<void(int)>;

    /**
     * @brief Middleware continuation callback.
     */
    using Next = std::function<void()>;

    /**
     * @brief Middleware signature used by the application.
     */
    using Middleware =
        std::function<void(vix::vhttp::Request &, vix::vhttp::ResponseWrapper &, Next)>;

    /**
     * @brief Signature of the injected static files handler.
     */
    using StaticHandler =
        std::function<bool(App &,
                           const std::filesystem::path &root,
                           const std::string &mount,
                           const std::string &index_file,
                           bool add_cache_control,
                           const std::string &cache_control,
                           bool fallthrough)>;

    /**
     * @brief Signature of the optional module initialization hook.
     */
    using ModuleInitFn = void (*)();

    /**
     * @brief Constructs an application with a default runtime executor.
     */
    App();

    /**
     * @brief Constructs an application with an externally provided runtime executor.
     *
     * @param executor Shared runtime executor used by the application.
     */
    explicit App(std::shared_ptr<vix::executor::RuntimeExecutor> executor);

    /**
     * @brief Destroys the application and releases owned resources.
     */
    ~App();

    App(const App &) = delete;
    App &operator=(const App &) = delete;
    App(App &&) = delete;
    App &operator=(App &&) = delete;

    /**
     * @brief Starts the server and blocks until shutdown.
     *
     * @param port Listening port.
     */
    void run(int port = 8080);

    /**
     * @brief Starts listening asynchronously.
     *
     * @param port Listening port.
     * @param on_listen Optional callback executed once the server is listening.
     */
    void listen(int port = 8080, ListenCallback on_listen = {});

    /**
     * @brief Waits for the running server to terminate.
     */
    void wait();

    /**
     * @brief Stops the server and triggers shutdown logic.
     */
    void close();

    /**
     * @brief Sets the shutdown callback.
     *
     * @param cb Callback invoked during shutdown.
     */
    void set_shutdown_callback(ShutdownCallback cb)
    {
      shutdown_cb_ = std::move(cb);
    }

    /**
     * @brief Registers a GET route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void get(const std::string &path, Handler handler)
    {
      add_route(http::verb::get, path, std::move(handler));
    }

    /**
     * @brief Registers a POST route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void post(const std::string &path, Handler handler)
    {
      add_route(http::verb::post, path, std::move(handler));
    }

    /**
     * @brief Registers a PUT route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void put(const std::string &path, Handler handler)
    {
      add_route(http::verb::put, path, std::move(handler));
    }

    /**
     * @brief Registers a PATCH route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void patch(const std::string &path, Handler handler)
    {
      add_route(http::verb::patch, path, std::move(handler));
    }

    /**
     * @brief Registers a DELETE route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void del(const std::string &path, Handler handler)
    {
      add_route(http::verb::delete_, path, std::move(handler));
    }

    /**
     * @brief Registers a HEAD route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void head(const std::string &path, Handler handler)
    {
      add_route(http::verb::head, path, std::move(handler));
    }

    /**
     * @brief Registers an OPTIONS route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void options(const std::string &path, Handler handler)
    {
      add_route(http::verb::options, path, std::move(handler));
    }

    /**
     * @brief Registers a heavy GET route.
     *
     * Heavy routes are marked with RouteOptions{ .heavy = true } so the runtime
     * can treat them differently when needed.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void get_heavy(const std::string &path, Handler handler)
    {
      add_route(http::verb::get, path, std::move(handler), vix::router::RouteOptions{.heavy = true});
    }

    /**
     * @brief Registers a heavy POST route.
     *
     * @tparam Handler Route handler type.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void post_heavy(const std::string &path, Handler handler)
    {
      add_route(http::verb::post, path, std::move(handler), vix::router::RouteOptions{.heavy = true});
    }

    /**
     * @brief Returns the mutable application configuration.
     *
     * @return vix::config::Config& Application configuration.
     */
    vix::config::Config &config() noexcept
    {
      return config_;
    }

    /**
     * @brief Returns the shared router.
     *
     * @return std::shared_ptr<vix::router::Router> Router instance.
     */
    std::shared_ptr<vix::router::Router> router() const noexcept
    {
      return router_;
    }

    /**
     * @brief Returns the underlying HTTP server.
     *
     * @return vix::server::HTTPServer& HTTP server instance.
     */
    vix::server::HTTPServer &server() noexcept
    {
      return server_;
    }

    /**
     * @brief Returns the runtime executor used by the application.
     *
     * @return vix::executor::RuntimeExecutor& Runtime executor.
     */
    vix::executor::RuntimeExecutor &executor() noexcept
    {
      return *executor_;
    }

    /**
     * @brief Indicates whether the server has started.
     *
     * @return true if running, false otherwise.
     */
    bool is_running() const noexcept
    {
      return started_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Requests a stop initiated by a signal handler.
     */
    void request_stop_from_signal() noexcept;

    /**
     * @brief Starts listening on a given port and notifies through a callback.
     *
     * @param port Listening port.
     * @param cb Optional callback receiving the bound port.
     */
    void listen_port(int port, ListenPortCallback cb = {});

    /**
     * @brief Enables or disables development mode.
     *
     * @param v Development mode state.
     */
    void setDevMode(bool v)
    {
      dev_mode_ = v;
    }

    /**
     * @brief Returns whether development mode is enabled.
     *
     * @return true if enabled, false otherwise.
     */
    bool isDevMode() const
    {
      return dev_mode_;
    }

    /**
     * @brief Installs a static file handler implementation.
     *
     * @param fn Static file handler.
     */
    static void set_static_handler(StaticHandler fn);

    /**
     * @brief Mounts a static directory.
     *
     * @param root Filesystem root to expose.
     * @param mount URL mount point.
     * @param index_file Default index filename.
     * @param add_cache_control Whether to add Cache-Control.
     * @param cache_control Cache-Control header value.
     * @param fallthrough Whether to continue routing if no file matches.
     */
    void static_dir(std::filesystem::path root,
                    std::string mount = "/",
                    std::string index_file = "index.html",
                    bool add_cache_control = true,
                    std::string cache_control = "public, max-age=3600",
                    bool fallthrough = true);

    /**
     * @brief Installs a global module initialization hook.
     *
     * @param fn Module initialization function.
     */
    static void set_module_init(ModuleInitFn fn);

    /**
     * @brief Route group helper.
     *
     * A group prefixes all registered routes and middleware with a common path.
     */
    class Group
    {
    public:
      /**
       * @brief Constructs a group bound to an application and a path prefix.
       *
       * @param app Parent application.
       * @param prefix Group route prefix.
       */
      Group(App &app, std::string prefix)
          : app_(&app), prefix_(normalize_prefix(std::move(prefix)))
      {
      }

      /**
       * @brief Creates a nested route group.
       *
       * @tparam Fn Function receiving the child group.
       * @param sub Sub-prefix.
       * @param fn Function called with the nested group.
       */
      template <class Fn>
      void group(std::string sub, Fn fn)
      {
        Group g(*app_, join(prefix_, std::move(sub)));
        fn(g);
      }

      /**
       * @brief Adds middleware to this group prefix.
       *
       * @param mw Middleware function.
       * @return Group& Current group.
       */
      Group &use(Middleware mw)
      {
        app_->use(prefix_, std::move(mw));
        return *this;
      }

      /**
       * @brief Protects a sub-prefix using middleware.
       *
       * @param sub_prefix Sub-prefix to protect.
       * @param mw Middleware function.
       * @return Group& Current group.
       */
      Group &protect(std::string sub_prefix, Middleware mw)
      {
        app_->protect(join(prefix_, std::move(sub_prefix)), std::move(mw));
        return *this;
      }

      /**
       * @brief Protects one exact path using middleware.
       *
       * @param sub_path Exact sub-path.
       * @param mw Middleware function.
       * @return Group& Current group.
       */
      Group &protect_exact(std::string sub_path, Middleware mw)
      {
        app_->protect_exact(join(prefix_, std::move(sub_path)), std::move(mw));
        return *this;
      }

      /**
       * @brief Registers a GET route within the group.
       *
       * @tparam Handler Route handler type.
       * @param path Route path relative to the group.
       * @param handler Route handler.
       */
      template <typename Handler>
      void get(const std::string &path, Handler handler)
      {
        app_->get(join(prefix_, path), std::move(handler));
      }

      /**
       * @brief Registers a POST route within the group.
       *
       * @tparam Handler Route handler type.
       * @param path Route path relative to the group.
       * @param handler Route handler.
       */
      template <typename Handler>
      void post(const std::string &path, Handler handler)
      {
        app_->post(join(prefix_, path), std::move(handler));
      }

      /**
       * @brief Registers a PUT route within the group.
       *
       * @tparam Handler Route handler type.
       * @param path Route path relative to the group.
       * @param handler Route handler.
       */
      template <typename Handler>
      void put(const std::string &path, Handler handler)
      {
        app_->put(join(prefix_, path), std::move(handler));
      }

      /**
       * @brief Registers a PATCH route within the group.
       *
       * @tparam Handler Route handler type.
       * @param path Route path relative to the group.
       * @param handler Route handler.
       */
      template <typename Handler>
      void patch(const std::string &path, Handler handler)
      {
        app_->patch(join(prefix_, path), std::move(handler));
      }

      /**
       * @brief Registers a DELETE route within the group.
       *
       * @tparam Handler Route handler type.
       * @param path Route path relative to the group.
       * @param handler Route handler.
       */
      template <typename Handler>
      void del(const std::string &path, Handler handler)
      {
        app_->del(join(prefix_, path), std::move(handler));
      }

    private:
      /**
       * @brief Normalizes a prefix to a clean absolute route prefix.
       *
       * @param p Raw prefix.
       * @return std::string Normalized prefix.
       */
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

      /**
       * @brief Joins a base prefix with a sub-path.
       *
       * @param base Base prefix.
       * @param sub Sub-path.
       * @return std::string Joined path.
       */
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

    /**
     * @brief Creates a temporary group and invokes a function with it.
     *
     * @tparam Fn Function receiving the group.
     * @param prefix Group prefix.
     * @param fn Function invoked with the group.
     */
    template <class Fn>
    void group(std::string prefix, Fn fn)
    {
      Group g(*this, std::move(prefix));
      fn(g);
    }

    /**
     * @brief Returns a reusable group object.
     *
     * @param prefix Group prefix.
     * @return Group Group instance.
     */
    Group group(std::string prefix)
    {
      return Group(*this, std::move(prefix));
    }

    /**
     * @brief Adds a global middleware.
     *
     * @param mw Middleware function.
     */
    void use(Middleware mw);

    /**
     * @brief Adds a middleware bound to a prefix.
     *
     * @param prefix Route prefix.
     * @param mw Middleware function.
     */
    void use(std::string prefix, Middleware mw);

    /**
     * @brief Protects a route prefix with middleware.
     *
     * @param prefix Prefix to protect.
     * @param mw Middleware function.
     */
    void protect(std::string prefix, Middleware mw)
    {
      use(normalize_prefix(std::move(prefix)), std::move(mw));
    }

    /**
     * @brief Protects one exact path with middleware.
     *
     * @param path Exact path to protect.
     * @param mw Middleware function.
     */
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

    /**
     * @brief Returns the last captured server ready information.
     *
     * @return vix::utils::ServerReadyInfo Last ready info.
     */
    vix::utils::ServerReadyInfo server_ready_info() const
    {
      std::lock_guard<std::mutex> lock(ready_info_mutex_);
      return last_ready_info_;
    }

    /**
     * @brief Indicates whether ready information has already been captured.
     *
     * @return true if available, false otherwise.
     */
    bool has_server_ready_info() const noexcept
    {
      return has_ready_info_.load(std::memory_order_relaxed);
    }

  private:
    /**
     * @brief Internal middleware registration entry.
     */
    struct MiddlewareEntry
    {
      std::string prefix;
      Middleware mw;
    };

    using RawRequestT = vix::vhttp::RawRequest;

    template <class H>
    static constexpr bool is_facade_handler_v =
        std::is_invocable_v<H &, vix::vhttp::Request &, vix::vhttp::ResponseWrapper &>;

    template <class H>
    static constexpr bool is_raw_handler_v =
        std::is_invocable_v<H &, const RawRequestT &, vix::vhttp::ResponseWrapper &>;

    /**
     * @brief Registers a route using default route options.
     *
     * @tparam Handler Route handler type.
     * @param method HTTP method.
     * @param path Route path.
     * @param handler Route handler.
     */
    template <typename Handler>
    void add_route(http::verb method, const std::string &path, Handler handler)
    {
      add_route(method, path, std::move(handler), vix::router::RouteOptions{});
    }

    /**
     * @brief Registers a route with explicit route options.
     *
     * Supported handlers are:
     * - (vix::vhttp::Request&, vix::vhttp::ResponseWrapper&)
     * - (const vix::vhttp::RawRequest&, vix::vhttp::ResponseWrapper&)
     *
     * @tparam Handler Route handler type.
     * @param method HTTP method.
     * @param path Route path.
     * @param handler Route handler.
     * @param opt Route options.
     */
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

    /**
     * @brief Returns whether a path matches a middleware prefix.
     *
     * @param prefix Middleware prefix.
     * @param path Request path.
     * @return true if matched, false otherwise.
     */
    bool match_middleware_prefix_(const std::string &prefix, const std::string &path) const;

    /**
     * @brief Collects all middleware that apply to a given path.
     *
     * @param path Request path.
     * @return std::vector<Middleware> Ordered middleware chain.
     */
    std::vector<Middleware> collect_middlewares_for_(const std::string &path) const;

    /**
     * @brief Executes a middleware chain recursively.
     *
     * @param chain Middleware chain.
     * @param i Current middleware index.
     * @param req Request wrapper.
     * @param res Response wrapper.
     * @param final_handler Final route callback.
     */
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

    /**
     * @brief Normalizes a route prefix or exact path.
     *
     * @param p Raw prefix/path.
     * @return std::string Normalized absolute path.
     */
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

    /**
     * @brief Automatically installs an OPTIONS route for a path when missing.
     *
     * @param path Route path.
     */
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

  private:
    vix::config::Config config_;
    std::shared_ptr<vix::router::Router> router_;
    std::shared_ptr<vix::executor::RuntimeExecutor> executor_;
    vix::server::HTTPServer server_;

    ShutdownCallback shutdown_cb_{};

    std::thread server_thread_;
    std::atomic<bool> started_{false};
    std::atomic<bool> stop_requested_{false};

    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;

    bool dev_mode_{false};

    std::atomic<bool> wait_called_{false};
    std::atomic<bool> listen_called_{false};

    mutable std::mutex ready_info_mutex_;
    vix::utils::ServerReadyInfo last_ready_info_{};
    std::atomic<bool> has_ready_info_{false};

    std::vector<MiddlewareEntry> middlewares_;
  };

} // namespace vix

#endif // VIX_HTTP_APP_HPP
