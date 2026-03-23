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

      (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
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

  void HTTPServer::init_acceptor(unsigned short port)
  {
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

    if (!acceptor_ || !acceptor_->is_open())
    {
      const int port = config_.getServerPort();

      if ((port != 0 && port < 1024) || port > 65535)
      {
        log().log(Logger::Level::Error,
                  "Server port {} out of range (1024-65535)",
                  port);
        throw std::invalid_argument("Invalid port number");
      }

      init_acceptor(static_cast<unsigned short>(port));
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

    auto socket = std::make_shared<tcp::socket>(*io_context_);

    acceptor_->async_accept(
        *socket,
        [this, socket](boost::system::error_code ec)
        {
          if (ec)
          {
            if (ec != net::error::operation_aborted &&
                !stop_requested_.load(std::memory_order_acquire))
            {
              log().log(Logger::Level::Error,
                        "Accept error: {}",
                        ec.message());
            }
            return;
          }

          if (stop_requested_.load(std::memory_order_acquire))
          {
            close_socket(socket);
            return;
          }

          handle_client(socket);

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
      return;
    }

    try
    {
      auto session = std::make_shared<vix::session::Session>(
          socket_ptr,
          *router_,
          config_,
          executor_);

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
    ec.clear();
    socket->close(ec);
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

    boost::system::error_code ec;

    if (acceptor_ && acceptor_->is_open())
    {
      acceptor_->cancel(ec);
      ec.clear();
      acceptor_->close(ec);
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
    }
  }

} // namespace vix::server
