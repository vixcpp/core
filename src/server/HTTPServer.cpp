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

      listener_->listen(ep);

      bound_port_.store(static_cast<int>(port), std::memory_order_relaxed);

      log().log(Logger::Level::Debug,
                "[http] listener bound on {}:{}",
                ep.host,
                static_cast<unsigned int>(port));
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

    start_io_threads();
    start_accept();
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

  vix::async::core::task<void> HTTPServer::accept_loop()
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
    const bool already_stopping =
        stop_requested_.exchange(true, std::memory_order_acq_rel);

    if (already_stopping)
    {
      return;
    }

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

    if (io_context_)
    {
      io_context_->stop();
    }
  }

  void HTTPServer::join_threads()
  {
    std::lock_guard<std::mutex> lock(join_mutex_);

    if (threads_joined_.load(std::memory_order_acquire))
    {
      log().log(Logger::Level::Debug, "[http] join_threads: already joined");
      return;
    }

    log().log(Logger::Level::Debug, "[http] join_threads: begin");

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
        log().log(Logger::Level::Debug, "[http] joining metrics_thread");
        metrics_thread_.join();
        log().log(Logger::Level::Debug, "[http] joined metrics_thread");
      }
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
        io_threads_[i].detach();
        deferred_completion = true;
        continue;
      }

      log().log(Logger::Level::Debug, "[http] joining io thread {}", i);
      io_threads_[i].join();
      log().log(Logger::Level::Debug, "[http] joined io thread {}", i);
    }

    io_threads_.clear();

    if (!deferred_completion)
    {
      threads_joined_.store(true, std::memory_order_release);
      log().log(Logger::Level::Debug, "[http] join_threads: fully completed");
    }
    else
    {
      log().log(Logger::Level::Debug,
                "[http] join_threads: deferred final completion");
    }

    log().log(Logger::Level::Debug, "[http] join_threads: end");
  }

  void HTTPServer::stop_blocking()
  {
    stop_async();
    join_threads();
  }
} // namespace vix::server
