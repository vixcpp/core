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

    void set_affinity(std::size_t thread_index)
    {
#if !defined(__linux__)
      (void)thread_index;
#else
      unsigned int hc = std::thread::hardware_concurrency();
      if (hc == 0u)
      {
        hc = 1u;
      }

      const unsigned int cpu =
          static_cast<unsigned int>(thread_index % static_cast<std::size_t>(hc));

      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(cpu, &cpuset);

      (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    }
  } // namespace

  HTTPServer::HTTPServer(vix::config::Config &config,
                         std::shared_ptr<vix::executor::RuntimeExecutor> exec)
      : config_(config),
        io_context_(std::make_shared<vix::async::core::io_context>()),
        listener_(nullptr),
        router_(std::make_shared<vix::router::Router>()),
        executor_(std::move(exec)),
        io_threads_(),
        metrics_thread_(),
        stop_requested_(false),
        startup_t0_(std::chrono::steady_clock::now()),
        bound_port_(0)
  {
    if (!executor_)
    {
      throw std::invalid_argument("HTTPServer requires a valid runtime executor");
    }

    try
    {
      router_->setNotFoundHandler(
          [](const vix::vhttp::Request &req,
             vix::vhttp::Response &res) -> vix::async::core::task<void>
          {
            res.set_status(vix::vhttp::NOT_FOUND);

            nlohmann::json j{
                {"error", "Route not found"},
                {"hint", "Check path, method, or API version"},
                {"method", req.method()},
                {"path", req.target()}};

            vix::vhttp::Response::json_response(res, j, vix::vhttp::NOT_FOUND);
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
      stop_blocking();
    }
    catch (...)
    {
    }
  }

  vix::async::net::tcp_endpoint HTTPServer::make_bind_endpoint() const
  {
    vix::async::net::tcp_endpoint ep{};
    ep.host = "0.0.0.0";
    ep.port = static_cast<std::uint16_t>(config_.getServerPort());
    return ep;
  }

  void HTTPServer::init_listener(unsigned short port)
  {
    listener_ = vix::async::net::make_tcp_listener(*io_context_);
    if (!listener_)
    {
      throw std::runtime_error("failed to create native Vix TCP listener");
    }

    try
    {
      vix::async::net::tcp_endpoint ep{};
      ep.host = "0.0.0.0";
      ep.port = port;

      auto t = listener_->async_listen(ep);
      std::move(t).start(io_context_->get_scheduler());

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
  }

  std::size_t HTTPServer::calculate_io_thread_count()
  {
    const int forced = config_.getIOThreads();
    if (forced > 0)
    {
      return static_cast<std::size_t>(forced);
    }

    unsigned int hc = std::thread::hardware_concurrency();
    if (hc == 0u)
    {
      hc = static_cast<unsigned int>(NUMBER_OF_THREADS);
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
              set_affinity(i);
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
      throw std::runtime_error("runtime executor is null");
    }

    executor_->start();

    if (!listener_)
    {
      const int port = config_.getServerPort();

      if ((port != 0 && port < 1024) || port > 65535)
      {
        log().log(Logger::Level::Error,
                  "Server port {} out of range (1024-65535)",
                  port);
        throw std::invalid_argument("Invalid port number");
      }

      init_listener(static_cast<unsigned short>(port));
    }

    start_accept();
    start_io_threads();
    monitor_metrics();
  }

  void HTTPServer::start_accept()
  {
    if (stop_requested_.load(std::memory_order_acquire))
    {
      return;
    }

    spawn_detached(*io_context_, accept_loop());
  }

  vix::async::core::task<void> HTTPServer::accept_loop()
  {
    while (!stop_requested_.load(std::memory_order_acquire))
    {
      try
      {
        auto stream = co_await listener_->async_accept();

        if (!stream)
        {
          continue;
        }

        if (stop_requested_.load(std::memory_order_acquire))
        {
          close_stream(std::move(stream));
          co_return;
        }

        spawn_detached(*io_context_, handle_client(std::move(stream)));
      }
      catch (const std::exception &e)
      {
        if (!stop_requested_.load(std::memory_order_acquire))
        {
          log().log(Logger::Level::Error,
                    "Accept error: {}",
                    e.what());
        }

        if (stop_requested_.load(std::memory_order_acquire))
        {
          break;
        }
      }
    }

    co_return;
  }

  vix::async::core::task<void> HTTPServer::handle_client(std::unique_ptr<tcp_stream> stream)
  {
    if (!stream)
    {
      co_return;
    }

    try
    {
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
          while (!stop_requested_.load(std::memory_order_acquire))
          {
            for (int i = 0; i < 50; ++i)
            {
              if (stop_requested_.load(std::memory_order_acquire))
              {
                return;
              }

              std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
          }
        });
  }

  void HTTPServer::stop_async()
  {
    const bool already_stopping =
        stop_requested_.exchange(true, std::memory_order_acq_rel);

    if (already_stopping)
    {
      return;
    }

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

    if (io_context_)
    {
      io_context_->stop();
    }
  }

  void HTTPServer::join_threads()
  {
    if (io_context_)
    {
      io_context_->stop();
    }

    for (auto &t : io_threads_)
    {
      if (t.joinable())
      {
        t.join();
      }
    }

    io_threads_.clear();

    if (metrics_thread_.joinable())
    {
      metrics_thread_.join();
    }
  }

  void HTTPServer::stop_blocking()
  {
    const bool was_already_stopping =
        stop_requested_.exchange(true, std::memory_order_acq_rel);

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

    join_threads();

    if (executor_)
    {
      executor_->stop();
    }

    if (!was_already_stopping)
    {
      const auto uptime_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - startup_t0_)
              .count();

      (void)uptime_ms;
    }
  }

} // namespace vix::server
