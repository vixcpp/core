/**
 * @file HTTPServer.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_HTTP_SERVER_HPP
#define VIX_HTTP_SERVER_HPP

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <vix/config/Config.hpp>
#include <vix/executor/IExecutor.hpp>
#include <vix/router/Router.hpp>

namespace vix::server
{
  namespace beast = boost::beast;
  namespace http = boost::beast::http;
  namespace net = boost::asio;
  using tcp = net::ip::tcp;

  /** @brief Default maximum number of I/O threads used by the HTTP server. */
  constexpr std::size_t NUMBER_OF_THREADS = 8;

  /** @brief Asynchronous HTTP server built on Boost.Asio and Boost.Beast. */
  class HTTPServer
  {
  public:
    /** @brief Create an HTTP server using the given configuration and executor. */
    explicit HTTPServer(vix::config::Config &config,
                        std::shared_ptr<vix::executor::IExecutor> exec);

    /** @brief Stop the server and release all resources. */
    ~HTTPServer();

    /** @brief Start the server event loop and begin accepting connections. */
    void run();

    /** @brief Start accepting incoming TCP connections. */
    void start_accept();

    /** @brief Compute the number of I/O threads to use based on configuration and hardware. */
    std::size_t calculate_io_thread_count();

    /** @brief Return the router used to dispatch incoming HTTP requests. */
    std::shared_ptr<vix::router::Router> getRouter() { return router_; }

    /** @brief Periodically collect and log runtime metrics. */
    void monitor_metrics();

    /** @brief Request an asynchronous server shutdown. */
    void stop_async();

    /** @brief Join all internal I/O threads. */
    void join_threads();

    /** @brief Return true if a stop has been requested. */
    bool is_stop_requested() const { return stop_requested_.load(); }

    /** @brief Stop the server and block until all threads have exited. */
    void stop_blocking();

    /**
     * @brief Return the actual TCP port bound by the acceptor.
     *
     * This is useful when the configured port was 0 (ephemeral port).
     * Returns 0 if the server is not bound yet.
     */
    int bound_port() const noexcept
    {
      return bound_port_.load(std::memory_order_relaxed);
    }

  private:
    /** @brief Initialize the TCP acceptor on the given port. */
    void init_acceptor(unsigned short port);

    /** @brief Handle a single client connection and process HTTP requests. */
    void handle_client(std::shared_ptr<tcp::socket> socket_ptr);

    /** @brief Close a client socket safely. */
    void close_socket(std::shared_ptr<tcp::socket> socket);

    /** @brief Launch the configured number of I/O worker threads. */
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
    std::atomic<int> bound_port_{0};
  };

} // namespace vix::server

#endif // VIX_HTTP_SERVER_HPP
