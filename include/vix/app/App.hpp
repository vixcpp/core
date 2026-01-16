/**
 *
 *  @file App.hpp
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

#ifndef VIX_HTTP_APP_HPP
#define VIX_HTTP_APP_HPP

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
  class Router;
}

namespace vix
{
  namespace http = boost::beast::http;
  using Logger = vix::utils::Logger;

  inline Logger &log()
  {
    return Logger::getInstance();
  }

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

    class Group
    {
    public:
      Group(App &app, std::string prefix)
          : app_(&app), prefix_(normalize_prefix(std::move(prefix)))
      {
      }

      template <class Fn>
      void group(std::string sub, Fn fn)
      {
        Group g(*app_, join(prefix_, std::move(sub)));
        fn(g);
      }

      Group &use(Middleware mw)
      {
        app_->use(prefix_, std::move(mw));
        return *this;
      }

      Group &protect(std::string sub_prefix, Middleware mw)
      {
        app_->protect(join(prefix_, std::move(sub_prefix)), std::move(mw));
        return *this;
      }

      Group &protect_exact(std::string sub_path, Middleware mw)
      {
        app_->protect_exact(join(prefix_, std::move(sub_path)), std::move(mw));
        return *this;
      }

      template <typename Handler>
      void get(const std::string &path, Handler handler)
      {
        app_->get(join(prefix_, path), std::move(handler));
      }

      template <typename Handler>
      void post(const std::string &path, Handler handler)
      {
        app_->post(join(prefix_, path), std::move(handler));
      }

      template <typename Handler>
      void put(const std::string &path, Handler handler)
      {
        app_->put(join(prefix_, path), std::move(handler));
      }

      template <typename Handler>
      void patch(const std::string &path, Handler handler)
      {
        app_->patch(join(prefix_, path), std::move(handler));
      }

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

    template <class Fn>
    void group(std::string prefix, Fn fn)
    {
      Group g(*this, std::move(prefix));
      fn(g);
    }

    Group group(std::string prefix)
    {
      return Group(*this, std::move(prefix));
    }

    void use(Middleware mw);
    void use(std::string prefix, Middleware mw);

    void protect(std::string prefix, Middleware mw)
    {
      use(normalize_prefix(std::move(prefix)), std::move(mw));
    }

    void protect_exact(std::string path, Middleware mw)
    {
      std::string match = normalize_prefix(std::move(path));
      use(match, [mw = std::move(mw), match](vix::vhttp::Request &req,
                                             vix::vhttp::ResponseWrapper &res,
                                             Next next) mutable
          {
        if (req.path() == match) mw(req, res, std::move(next));
        else next(); });
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

#endif // VIX_APP_HPP
