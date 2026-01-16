/**
 *
 *  @file HTTPServer.hpp
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
#ifndef VIX_HTTP_SERVER_HPP
#define VIX_HTTP_SERVER_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <thread>
#include <memory>
#include <vector>
#include <atomic>

#include <vix/router/Router.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/IExecutor.hpp>

namespace vix::server
{
  namespace beast = boost::beast;
  namespace http = boost::beast::http;
  namespace net = boost::asio;
  using tcp = net::ip::tcp;

  constexpr size_t NUMBER_OF_THREADS = 8;

  class HTTPServer
  {
  public:
    explicit HTTPServer(vix::config::Config &config, std::shared_ptr<vix::executor::IExecutor> exec);
    ~HTTPServer();
    void run();
    void start_accept();
    std::size_t calculate_io_thread_count();
    std::shared_ptr<vix::router::Router> getRouter() { return router_; }
    void monitor_metrics();
    void stop_async();
    void join_threads();
    bool is_stop_requested() const { return stop_requested_.load(); }
    void stop_blocking();

  private:
    void init_acceptor(unsigned short port);
    void handle_client(std::shared_ptr<tcp::socket> socket_ptr);
    void close_socket(std::shared_ptr<tcp::socket> socket);
    void start_io_threads();

  private:
    vix::config::Config &config_;
    std::shared_ptr<net::io_context> io_context_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    std::shared_ptr<vix::router::Router> router_;
    std::shared_ptr<vix::executor::IExecutor> executor_;
    std::vector<std::thread> io_threads_;
    std::atomic<bool> stop_requested_{false};
    std::chrono::steady_clock::time_point startup_t0_;
  };

} // namespace vix

#endif // VIX_HTTP_SERVER_HPP
