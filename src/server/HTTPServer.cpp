/**
 *
 *  @file HTTPServer.cpp
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
#include <vix/server/HTTPServer.hpp>

#include <boost/regex.hpp>
#include <string>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <system_error>
#include <boost/system/error_code.hpp>
#include <cstring>
#include <chrono>

#include <vix/session/Session.hpp>
#include <vix/http/Response.hpp>
#include <vix/threadpool/ThreadPool.hpp>
#include <vix/json/build.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/timers/interval.hpp>
#include <vix/executor/Metrics.hpp>
#include <vix/utils/ServerPrettyLogs.hpp>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace vix::server
{
  using Logger = vix::utils::Logger;

  void set_affinity(std::size_t thread_index)
  {
#ifdef __linux__
    unsigned int hc = std::thread::hardware_concurrency();
    if (hc == 0u)
      hc = 1u;

    const unsigned int cpu =
        static_cast<unsigned int>(thread_index % static_cast<std::size_t>(hc));

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
  }

  HTTPServer::HTTPServer(vix::config::Config &config, std::shared_ptr<vix::executor::IExecutor> exec)
      : config_(config),
        io_context_(std::make_shared<net::io_context>()),
        acceptor_(nullptr),
        router_(std::make_shared<vix::router::Router>()),
        executor_(std::move(exec)),
        io_threads_(),
        stop_requested_(false),
        startup_t0_(std::chrono::steady_clock::now())
  {
    auto &log = Logger::getInstance();
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
      log.log(Logger::Level::ERROR, "Error initializing HTTPServer: {}", e.what());
      throw;
    }
  }

  HTTPServer::~HTTPServer() = default;

  void HTTPServer::init_acceptor(unsigned short port)
  {
    auto &log = Logger::getInstance();
    acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);
    boost::system::error_code ec;

    tcp::endpoint endpoint(tcp::v4(), port);

    acceptor_->open(endpoint.protocol(), ec);
    if (ec)
      throw std::system_error(ec, "open acceptor");

    acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
      throw std::system_error(ec, "reuse_address");

    acceptor_->bind(endpoint, ec);
    if (ec)
    {
      if (ec == boost::system::errc::address_in_use)
      {
        throw std::system_error(
            ec,
            "bind acceptor: address already in use. Another process is listening on this port.");
      }
      throw std::system_error(ec, "bind acceptor");
    }

    acceptor_->listen(boost::asio::socket_base::max_connections, ec);
    if (ec)
      throw std::system_error(ec, "listen acceptor");
  }

  void HTTPServer::start_io_threads()
  {
    auto &log = Logger::getInstance();
    const std::size_t num_threads = calculate_io_thread_count();

    for (std::size_t i = 0; i < num_threads; ++i)
    {
      io_threads_.emplace_back(
          [this, i, &log]()
          {
            try
            {
              set_affinity(i);
              io_context_->run();
            }
            catch (const std::exception &e)
            {
              log.log(Logger::Level::ERROR, "Error in io_context thread {}: {}", i, e.what());
            }
            log.log(Logger::Level::DEBUG,
                    "[http] io thread {} finished", i);
          });
    }
  }

  void HTTPServer::run()
  {
    auto &log = Logger::getInstance();

    if (!acceptor_ || !acceptor_->is_open())
    {
      int port = config_.getServerPort();
      if (port < 1024 || port > 65535)
      {
        log.log(Logger::Level::ERROR, "Server port {} out of range (1024-65535)", port);
        throw std::invalid_argument("Invalid port number");
      }

      init_acceptor(static_cast<unsigned short>(port));
    }

    start_accept();
    monitor_metrics();
    start_io_threads();
  }

  std::size_t HTTPServer::calculate_io_thread_count()
  {
    int forced = config_.getIOThreads();
    if (forced > 0)
      return static_cast<std::size_t>(forced);

    unsigned int hc = std::thread::hardware_concurrency();
    if (hc == 0)
      hc = 1;
    return static_cast<std::size_t>(hc);
  }

  void HTTPServer::start_accept()
  {
    auto socket = std::make_shared<tcp::socket>(*io_context_);
    acceptor_->async_accept(*socket, [this, socket](boost::system::error_code ec)
                            {
        if (!ec && !stop_requested_)
        {
            handle_client(socket);
        }
        if (!stop_requested_)
            start_accept(); });
  }

  void HTTPServer::handle_client(std::shared_ptr<tcp::socket> socket_ptr)
  {
    auto session = std::make_shared<vix::session::Session>(
        socket_ptr, *router_, config_, executor_);
    session->run();
  }

  void HTTPServer::close_socket(std::shared_ptr<tcp::socket> socket)
  {
    boost::system::error_code ec;
    socket->shutdown(tcp::socket::shutdown_both, ec);
    socket->close(ec);
  }

  void HTTPServer::stop_async()
  {
    stop_requested_ = true;
    if (acceptor_ && acceptor_->is_open())
      acceptor_->close();
    io_context_->stop();
  }

  void HTTPServer::stop_blocking()
  {
    executor_->wait_idle();
    join_threads();
  }

  void HTTPServer::join_threads()
  {
    for (auto &t : io_threads_)
      if (t.joinable())
        t.join();
  }

  void HTTPServer::monitor_metrics()
  {
    vix::timers::interval(*executor_, std::chrono::seconds(5), [this]()
                          {
        const auto m = executor_->metrics();
        Logger::getInstance().log(Logger::Level::DEBUG,
            "Executor Metrics -> Pending: {}, Active: {}, TimedOut: {}",
            m.pending, m.active, m.timed_out); });
  }

} // namespace vix
