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

#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

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

      const int rc =
          pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

      if (rc != 0)
      {
        log().log(Logger::Level::Debug,
                  "[http] failed to pin io thread {} to cpu {}",
                  thread_index,
                  cpu);
      }
      else
      {
        log().log(Logger::Level::Debug,
                  "[http] pinned io thread {} to cpu {}",
                  thread_index,
                  cpu);
      }
#endif
    }
  } // namespace

  HTTPServer::HTTPServer(vix::config::Config &config,
                         std::shared_ptr<vix::executor::RuntimeExecutor> exec)
      : config_(config),
        io_context_(std::make_shared<net::io_context>()),
        acceptor_(nullptr),
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

    log().log(Logger::Level::Debug,
              "[http] HTTPServer ctor: executor ready");

    try
    {
      router_->setNotFoundHandler(
          [](const http::request<http::string_body> &req,
             http::response<http::string_body> &res)
          {
            res.result(http::status::not_found);

            if (req.method() == http::verb::head)
            {
              res.set(http::field::content_type, "application/json");
              res.set(http::field::connection, "close");
              res.body().clear();
              res.prepare_payload();
              return;
            }

            vix::json::Json j{
                {"error", "Route not found"},
                {"hint", "Check path, method, or API version"},
                {"method", std::string(req.method_string())},
                {"path", std::string(req.target())}};

            vix::vhttp::Response::json_response(res, j, res.result());
            res.set(http::field::connection, "close");
            res.prepare_payload();
          });

      log().log(Logger::Level::Debug,
                "[http] not-found handler installed");
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
      if (!stop_requested_.load(std::memory_order_acquire))
      {
        log().log(Logger::Level::Debug,
                  "[http] HTTPServer dtor: stop_blocking()");
        stop_blocking();
      }
    }
    catch (...)
    {
    }
  }

  void HTTPServer::init_acceptor(unsigned short port)
  {
    log().log(Logger::Level::Debug,
              "[http] init_acceptor(port={})",
              static_cast<unsigned int>(port));

    acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);

    boost::system::error_code ec;
    const tcp::endpoint endpoint(tcp::v4(), port);

    acceptor_->open(endpoint.protocol(), ec);
    if (ec)
    {
      log().log(Logger::Level::Error,
                "[http] acceptor open failed: {}",
                ec.message());
      throw std::system_error(ec, "open acceptor");
    }

    acceptor_->set_option(net::socket_base::reuse_address(true), ec);
    if (ec)
    {
      log().log(Logger::Level::Error,
                "[http] acceptor reuse_address failed: {}",
                ec.message());
      throw std::system_error(ec, "reuse_address");
    }

    acceptor_->bind(endpoint, ec);
    if (ec)
    {
      log().log(Logger::Level::Error,
                "[http] acceptor bind failed on port {}: {}",
                static_cast<unsigned int>(port),
                ec.message());

      if (ec == boost::system::errc::address_in_use)
      {
        throw std::system_error(
            ec,
            "bind acceptor: address already in use. Another process is listening on this port.");
      }

      throw std::system_error(ec, "bind acceptor");
    }

    acceptor_->listen(net::socket_base::max_listen_connections, ec);
    if (ec)
    {
      log().log(Logger::Level::Error,
                "[http] acceptor listen failed: {}",
                ec.message());
      throw std::system_error(ec, "listen acceptor");
    }

    boost::system::error_code ep_ec;
    const auto ep = acceptor_->local_endpoint(ep_ec);
    if (!ep_ec)
    {
      bound_port_.store(static_cast<int>(ep.port()), std::memory_order_relaxed);

      log().log(Logger::Level::Info,
                "[http] acceptor ready on {}:{}",
                ep.address().to_string(),
                ep.port());
    }
    else
    {
      log().log(Logger::Level::Debug,
                "[http] local_endpoint read failed: {}",
                ep_ec.message());
    }
  }

  std::size_t HTTPServer::calculate_io_thread_count()
  {
    const int forced = config_.getIOThreads();
    if (forced > 0)
    {
      log().log(Logger::Level::Debug,
                "[http] using configured io thread count: {}",
                forced);
      return static_cast<std::size_t>(forced);
    }

    unsigned int hc = std::thread::hardware_concurrency();
    if (hc == 0u)
    {
      hc = static_cast<unsigned int>(NUMBER_OF_THREADS);

      log().log(Logger::Level::Debug,
                "[http] hardware_concurrency returned 0, fallback={}",
                hc);
    }
    else
    {
      log().log(Logger::Level::Debug,
                "[http] hardware_concurrency={}",
                hc);
    }

    return static_cast<std::size_t>(hc);
  }

  void HTTPServer::start_io_threads()
  {
    const std::size_t num_threads = calculate_io_thread_count();

    log().log(Logger::Level::Info,
              "[http] starting {} io thread(s)",
              num_threads);

    io_threads_.reserve(num_threads);

    for (std::size_t i = 0; i < num_threads; ++i)
    {
      io_threads_.emplace_back(
          [this, i]()
          {
            log().log(Logger::Level::Debug,
                      "[http] io thread {} starting",
                      i);

            try
            {
              set_affinity(i);

              const std::size_t handled = io_context_->run();

              log().log(Logger::Level::Debug,
                        "[http] io thread {} run() exited, handlers={}",
                        i,
                        handled);
            }
            catch (const std::exception &e)
            {
              log().log(Logger::Level::Error,
                        "Error in io_context thread {}: {}",
                        i,
                        e.what());
            }

            log().log(Logger::Level::Debug,
                      "[http] io thread {} finished",
                      i);
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

    log().log(Logger::Level::Info,
              "[http] run() called");

    executor_->start();

    log().log(Logger::Level::Debug,
              "[http] runtime executor started");

    if (!acceptor_ || !acceptor_->is_open())
    {
      const int port = config_.getServerPort();

      log().log(Logger::Level::Debug,
                "[http] configured port={}",
                port);

      if ((port != 0 && port < 1024) || port > 65535)
      {
        log().log(Logger::Level::Error,
                  "Server port {} out of range (1024-65535)",
                  port);
        throw std::invalid_argument("Invalid port number");
      }

      init_acceptor(static_cast<unsigned short>(port));
    }
    else
    {
      log().log(Logger::Level::Debug,
                "[http] acceptor already initialized");
    }

    start_accept();
    start_io_threads();
    monitor_metrics();

    log().log(Logger::Level::Info,
              "HTTP server started on port {} using {} network io threads",
              bound_port(),
              io_threads_.size());
  }

  void HTTPServer::start_accept()
  {
    if (stop_requested_.load(std::memory_order_acquire))
    {
      log().log(Logger::Level::Debug,
                "[http] start_accept skipped because stop requested");
      return;
    }

    auto socket = std::make_shared<tcp::socket>(*io_context_);

    log().log(Logger::Level::Debug,
              "[http] scheduling async_accept");

    acceptor_->async_accept(
        *socket,
        [this, socket](boost::system::error_code ec)
        {
          if (stop_requested_.load(std::memory_order_acquire))
          {
            log().log(Logger::Level::Debug,
                      "[http] accept callback ignored because stop requested");
            return;
          }

          if (!ec)
          {
            boost::system::error_code remote_ec;
            const auto remote_ep = socket->remote_endpoint(remote_ec);

            if (!remote_ec)
            {
              log().log(Logger::Level::Debug,
                        "[http] accepted client {}:{}",
                        remote_ep.address().to_string(),
                        remote_ep.port());
            }
            else
            {
              log().log(Logger::Level::Debug,
                        "[http] accepted client but failed to read remote endpoint: {}",
                        remote_ec.message());
            }

            handle_client(socket);
          }
          else if (ec != net::error::operation_aborted)
          {
            log().log(Logger::Level::Error,
                      "Accept error: {}",
                      ec.message());
          }
          else
          {
            log().log(Logger::Level::Debug,
                      "[http] accept aborted");
          }

          if (!stop_requested_.load(std::memory_order_acquire))
          {
            start_accept();
          }
        });
  }

  void HTTPServer::handle_client(std::shared_ptr<tcp::socket> socket_ptr)
  {
    if (!socket_ptr)
    {
      log().log(Logger::Level::Debug,
                "[http] handle_client skipped: null socket");
      return;
    }

    try
    {
      auto session = std::make_shared<vix::session::Session>(
          socket_ptr,
          *router_,
          config_,
          executor_);

      log().log(Logger::Level::Debug,
                "[http] session created, starting run()");

      session->run();
    }
    catch (const std::exception &e)
    {
      log().log(Logger::Level::Error,
                "Failed to create session: {}",
                e.what());
      close_socket(std::move(socket_ptr));
    }
  }

  void HTTPServer::close_socket(std::shared_ptr<tcp::socket> socket)
  {
    if (!socket)
    {
      return;
    }

    boost::system::error_code ec;
    socket->shutdown(tcp::socket::shutdown_both, ec);

    if (ec)
    {
      log().log(Logger::Level::Debug,
                "[http] socket shutdown error: {}",
                ec.message());
    }

    ec.clear();
    socket->close(ec);

    if (ec)
    {
      log().log(Logger::Level::Debug,
                "[http] socket close error: {}",
                ec.message());
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
            const auto m = executor_->metrics();

            Logger::getInstance().log(
                Logger::Level::Debug,
                "Runtime Metrics -> Pending: {}, Active: {}, TimedOut: {}",
                m.pending,
                m.active,
                m.timed_out);

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
      log().log(Logger::Level::Debug,
                "[http] stop_async() ignored: already stopping");
      return;
    }

    log().log(Logger::Level::Info,
              "[http] stop_async() requested");

    boost::system::error_code ec;

    if (acceptor_ && acceptor_->is_open())
    {
      acceptor_->cancel(ec);
      if (ec)
      {
        log().log(Logger::Level::Debug,
                  "[http] acceptor cancel error: {}",
                  ec.message());
      }

      ec.clear();
      acceptor_->close(ec);
      if (ec)
      {
        log().log(Logger::Level::Debug,
                  "[http] acceptor close error: {}",
                  ec.message());
      }
    }
  }

  void HTTPServer::join_threads()
  {
    log().log(Logger::Level::Debug,
              "[http] join_threads() begin");

    if (io_context_)
    {
      io_context_->stop();
      log().log(Logger::Level::Debug,
                "[http] io_context stopped");
    }

    for (std::size_t i = 0; i < io_threads_.size(); ++i)
    {
      auto &t = io_threads_[i];
      if (t.joinable())
      {
        log().log(Logger::Level::Debug,
                  "[http] joining io thread {}",
                  i);
        t.join();
      }
    }

    io_threads_.clear();

    if (metrics_thread_.joinable())
    {
      log().log(Logger::Level::Debug,
                "[http] joining metrics thread");
      metrics_thread_.join();
    }

    log().log(Logger::Level::Debug,
              "[http] join_threads() end");
  }

  void HTTPServer::stop_blocking()
  {
    const bool was_already_stopping =
        stop_requested_.exchange(true, std::memory_order_acq_rel);

    if (was_already_stopping)
    {
      return;
    }

    log().log(Logger::Level::Info,
              "[http] stop_blocking() begin");

    boost::system::error_code ec;

    if (acceptor_ && acceptor_->is_open())
    {
      acceptor_->cancel(ec);
      ec.clear();
      acceptor_->close(ec);
    }

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

    if (executor_)
    {
      executor_->stop();
    }

    const auto uptime_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startup_t0_)
            .count();

    log().log(Logger::Level::Info,
              "[http] stop_blocking() done, uptime_ms={}",
              uptime_ms);
  }

} // namespace vix::server
