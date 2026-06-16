/**
 *
 * @file HTTPServer.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#include <vix/server/HTTPServer.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <nlohmann/json.hpp>

#include <vix/async/core/spawn.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/http/Response.hpp>
#include <vix/json/build.hpp>
#include <vix/session/Session.hpp>
#include <vix/session/TlsSession.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace vix::server
{
  using Logger = vix::utils::Logger;
  using vix::async::core::spawn_detached;

  namespace
  {
    inline Logger &log()
    {
      return Logger::getInstance();
    }
  } // namespace

  HTTPServer::HTTPServer(
      vix::config::Config &config,
      std::shared_ptr<vix::executor::RuntimeExecutor> executor)
      : config_(config),
        io_context_(std::make_shared<vix::async::core::io_context>()),
        listener_(nullptr),
        router_(std::make_shared<vix::router::Router>()),
        executor_(std::move(executor)),
        io_threads_(),
        metrics_thread_(),
        metrics_mutex_(),
        metrics_cv_(),
        join_mutex_(),
        threads_joined_(false),
        stop_requested_(false),
        accept_loop_started_(false),
        bound_port_(0),
        startup_t0_(std::chrono::steady_clock::now())
  {
    if (!executor_)
    {
      throw std::invalid_argument("HTTPServer requires a valid executor");
    }

    try
    {
      router_->setNotFoundHandler(
          [](const vix::http::Request &req,
             vix::http::Response &res) -> vix::async::core::task<void>
          {
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
    catch (const std::exception &e)
    {
      log().log(Logger::Level::Error,
                "Error initializing HTTPServer: {}",
                e.what());
      throw;
    }
  }

  HTTPServer::~HTTPServer()
  {
    try
    {
      stop_async();

      if (!threads_joined_.load(std::memory_order_acquire))
      {
        join_threads();
      }
    }
    catch (...)
    {
    }
  }

  tcp_endpoint HTTPServer::make_bind_endpoint() const
  {
    tcp_endpoint ep{};
    ep.host = "0.0.0.0";
    ep.port = static_cast<std::uint16_t>(config_.getServerPort());
    return ep;
  }

  TlsConfig HTTPServer::tls_config() const
  {
    return config_.getTlsConfig();
  }

  bool HTTPServer::tls_enabled() const
  {
    return tls_config().is_valid();
  }

  task<void> HTTPServer::init_listener(unsigned short port)
  {
    listener_ = vix::async::net::make_tcp_listener(*io_context_);
    if (!listener_)
    {
      throw std::runtime_error("failed to create native Vix TCP listener");
    }

    try
    {
      tcp_endpoint ep{};
      ep.host = "0.0.0.0";
      ep.port = port;

      co_await listener_->async_listen(ep);

      bound_port_.store(static_cast<int>(port), std::memory_order_relaxed);
    }
    catch (const std::exception &e)
    {
      log().log(Logger::Level::Error,
                "[http] listener init failed on port {}: {}",
                static_cast<unsigned int>(port),
                e.what());
      throw;
    }

    co_return;
  }

  task<void> HTTPServer::start_server()
  {
    const int port = config_.getServerPort();

    if ((port != 0 && port < 1024) || port > 65535)
    {
      log().log(Logger::Level::Error,
                "Server port {} out of range (1024-65535)",
                port);
      throw std::invalid_argument("Invalid port number");
    }

    if (!listener_)
    {
      co_await init_listener(static_cast<unsigned short>(port));
    }

    if (stop_requested_.load(std::memory_order_acquire))
    {
      co_return;
    }

    start_accept();
    co_return;
  }

  std::size_t HTTPServer::calculate_io_thread_count()
  {
    auto hc = std::thread::hardware_concurrency();
    if (hc == 0)
    {
      hc = 4;
    }

    return static_cast<std::size_t>(hc);
  }

  void HTTPServer::start_io_threads()
  {
    const std::size_t num_threads = calculate_io_thread_count();
    io_threads_.reserve(num_threads);

    for (std::size_t i = 0; i < num_threads; ++i)
    {
      io_threads_.emplace_back(
          [this, i]()
          {
            try
            {
              io_context_->run();
            }
            catch (const std::exception &e)
            {
              log().log(Logger::Level::Error,
                        "Error in io_context thread {}: {}",
                        i,
                        e.what());
            }
          });
    }
  }

  void HTTPServer::run()
  {
    if (stop_requested_.load(std::memory_order_acquire))
    {
      throw std::runtime_error("cannot run server after stop was requested");
    }

    if (!executor_)
    {
      throw std::runtime_error("executor is null");
    }

    start_io_threads();
    spawn_detached(*io_context_, start_server());
    monitor_metrics();
  }

  void HTTPServer::start_accept()
  {
    if (stop_requested_.load(std::memory_order_acquire))
    {
      return;
    }

    bool expected = false;
    if (!accept_loop_started_.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
      return;
    }

    spawn_detached(*io_context_, accept_loop());
  }

  bool HTTPServer::is_listener_open() const noexcept
  {
    return listener_ && listener_->is_open();
  }

  bool HTTPServer::should_silence_accept_error(const std::exception &e) const noexcept
  {
    if (stop_requested_.load(std::memory_order_acquire))
    {
      return true;
    }

    if (!is_listener_open())
    {
      return true;
    }

    const auto *se = dynamic_cast<const std::system_error *>(&e);
    if (!se)
    {
      return false;
    }

    const auto code = se->code();

    if (code == std::errc::operation_canceled ||
        code == std::errc::bad_file_descriptor)
    {
      return true;
    }

#ifdef ECANCELED
    if (code.value() == ECANCELED)
    {
      return true;
    }
#endif

#ifdef EBADF
    if (code.value() == EBADF)
    {
      return true;
    }
#endif

    return false;
  }

  task<void> HTTPServer::accept_loop()
  {
    while (!stop_requested_.load(std::memory_order_acquire))
    {
      if (!is_listener_open())
      {
        break;
      }

      try
      {
        auto stream = co_await listener_->async_accept();

        if (!stream)
        {
          if (stop_requested_.load(std::memory_order_acquire) || !is_listener_open())
          {
            break;
          }
          continue;
        }

        if (stop_requested_.load(std::memory_order_acquire))
        {
          close_stream(std::move(stream));
          break;
        }

        spawn_detached(*io_context_, handle_client(std::move(stream)));
      }
      catch (const std::exception &e)
      {
        if (should_silence_accept_error(e))
        {
          break;
        }

        log().log(Logger::Level::Error,
                  "Accept error: {}",
                  e.what());
      }
    }

    accept_loop_started_.store(false, std::memory_order_release);
    co_return;
  }

  task<void> HTTPServer::handle_client(std::unique_ptr<tcp_stream> stream)
  {
    if (!stream)
    {
      co_return;
    }

    try
    {
      if (tls_enabled())
      {
        auto session = std::make_shared<vix::session::TlsSession>(
            std::move(stream),
            *router_,
            config_,
            executor_);

        co_await session->run();
        co_return;
      }

      auto session = std::make_shared<vix::session::Session>(
          std::move(stream),
          *router_,
          config_,
          executor_);

      co_await session->run();
    }
    catch (const std::exception &e)
    {
      log().log(Logger::Level::Error,
                "Failed to create or run session: {}",
                e.what());

      close_stream(std::move(stream));
    }

    co_return;
  }

  void HTTPServer::close_stream(std::unique_ptr<tcp_stream> stream)
  {
    if (!stream)
    {
      return;
    }

    try
    {
      stream->close();
    }
    catch (...)
    {
    }
  }

  void HTTPServer::monitor_metrics()
  {
    if (!executor_)
    {
      return;
    }

    if (metrics_thread_.joinable())
    {
      return;
    }

    metrics_thread_ = std::thread(
        [this]()
        {
          std::unique_lock<std::mutex> lock(metrics_mutex_);

          while (!stop_requested_.load(std::memory_order_acquire))
          {
            metrics_cv_.wait_for(
                lock,
                std::chrono::seconds(5),
                [this]()
                {
                  return stop_requested_.load(std::memory_order_acquire);
                });
          }
        });
  }

  void HTTPServer::stop_async()
  {
    stop_requested_.store(true, std::memory_order_release);

    metrics_cv_.notify_all();

    try
    {
      if (listener_)
      {
        listener_->close();
      }
    }
    catch (...)
    {
    }

    /*
     * Important:
     *
     * Do not stop io_context_ here.
     *
     * listener_->close() is supposed to cancel the pending async_accept().
     * The accept completion must still be allowed to resume accept_loop(),
     * let it catch/observe the shutdown error, store accept_loop_started_=false,
     * and reach co_return.
     *
     * io_context_ is stopped later in join_threads(), after the accept loop
     * has had a chance to drain.
     */
  }

  void HTTPServer::join_threads()
  {
    std::lock_guard<std::mutex> lock(join_mutex_);

    if (threads_joined_.load(std::memory_order_acquire))
    {
      return;
    }

    const std::thread::id current_id = std::this_thread::get_id();
    bool deferred_completion = false;

    if (metrics_thread_.joinable())
    {
      if (metrics_thread_.get_id() == current_id)
      {
        log().log(Logger::Level::Warn,
                  "[http] join_threads: metrics self thread detached");
        metrics_thread_.detach();
        deferred_completion = true;
      }
      else
      {
        metrics_thread_.join();
      }
    }

    bool called_from_io_thread = false;

    for (const auto &t : io_threads_)
    {
      if (t.joinable() && t.get_id() == current_id)
      {
        called_from_io_thread = true;
        break;
      }
    }

    if (!called_from_io_thread)
    {
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(2);

      while (accept_loop_started_.load(std::memory_order_acquire) &&
             std::chrono::steady_clock::now() < deadline)
      {
        try
        {
          if (listener_)
          {
            listener_->close();
          }
        }
        catch (...)
        {
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      if (accept_loop_started_.load(std::memory_order_acquire))
      {
        log().log(Logger::Level::Warn,
                  "[http] accept loop did not drain before io_context stop");
      }
    }

    if (io_context_)
    {
      io_context_->stop();
    }

    for (std::size_t i = 0; i < io_threads_.size(); ++i)
    {
      if (!io_threads_[i].joinable())
      {
        continue;
      }

      if (io_threads_[i].get_id() == current_id)
      {
        log().log(Logger::Level::Warn,
                  "[http] join_threads: detaching current io thread {}",
                  i);
        deferred_completion = true;
        io_threads_[i].detach();
        continue;
      }

      io_threads_[i].join();
    }

    io_threads_.clear();

    if (!deferred_completion && io_context_)
    {
      io_context_->shutdown();
    }

    if (!deferred_completion)
    {
      threads_joined_.store(true, std::memory_order_release);
    }
  }

  void HTTPServer::stop_blocking()
  {
    stop_async();
    join_threads();
  }
} // namespace vix::server
