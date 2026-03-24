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
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include <vix/async/core/io_context.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>

namespace vix::server
{
  using vix::async::core::io_context;
  using vix::async::core::task;
  using vix::async::net::tcp_endpoint;
  using vix::async::net::tcp_listener;
  using vix::async::net::tcp_stream;

  /** @brief Default maximum number of CPU worker threads used by the HTTP server runtime. */
  constexpr std::size_t NUMBER_OF_THREADS = 8;

  /**
   * @brief Native asynchronous HTTP server built on Vix async.
   *
   * This server no longer depends on Boost.Asio or Boost.Beast.
   * Networking is provided by vix::async::net and orchestration by vix::async::core.
   */
  class HTTPServer
  {
  public:
    /**
     * @brief Create an HTTP server using the given configuration and runtime executor.
     *
     * @param config Server configuration.
     * @param exec Runtime-backed executor used for application work.
     */
    explicit HTTPServer(
        vix::config::Config &config,
        std::shared_ptr<vix::executor::RuntimeExecutor> exec);

    /** @brief Stop the server and release all resources. */
    ~HTTPServer();

    /**
     * @brief Start the server runtime and begin accepting TCP connections.
     *
     * This typically starts the Vix async runtime, binds the listener,
     * launches the accept loop, and starts background monitoring threads.
     */
    void run();

    /**
     * @brief Start accepting incoming TCP connections.
     *
     * This is kept as a public API for compatibility with the previous server shape.
     * Internally it launches the Vix async accept loop.
     */
    void start_accept();

    /** @brief Compute the number of CPU worker threads to use based on configuration and hardware. */
    std::size_t calculate_io_thread_count();

    /** @brief Return the router used to dispatch incoming HTTP requests. */
    std::shared_ptr<vix::router::Router> getRouter()
    {
      return router_;
    }

    /** @brief Periodically collect and log runtime metrics. */
    void monitor_metrics();

    /** @brief Request an asynchronous server shutdown. */
    void stop_async();

    /** @brief Join all internal worker threads. */
    void join_threads();

    /** @brief Return true if a stop has been requested. */
    bool is_stop_requested() const
    {
      return stop_requested_.load(std::memory_order_acquire);
    }

    /** @brief Stop the server and block until all threads have exited. */
    void stop_blocking();

    /**
     * @brief Return the actual TCP port bound by the listener.
     *
     * This is useful when the configured port was 0 (ephemeral port).
     * Returns 0 if the server is not bound yet.
     */
    int bound_port() const noexcept
    {
      return bound_port_.load(std::memory_order_relaxed);
    }

  private:
    /** @brief Initialize the native Vix TCP listener on the given port. */
    void init_listener(unsigned short port);

    /**
     * @brief Main asynchronous accept loop.
     *
     * Accepts new TCP connections from the listener and dispatches each connection
     * to handle_client().
     */
    task<void> accept_loop();

    /**
     * @brief Handle a single client connection and process HTTP requests.
     *
     * The implementation is expected to:
     * - read raw HTTP data from the stream
     * - parse it into a native vix::vhttp::Request
     * - dispatch through the router
     * - serialize and write a native vix::vhttp::Response
     */
    task<void> handle_client(std::unique_ptr<tcp_stream> stream);

    /** @brief Close a client stream safely. */
    void close_stream(std::unique_ptr<tcp_stream> stream);

    /** @brief Launch the configured number of CPU worker threads for runtime support. */
    void start_io_threads();

    /** @brief Return the bind endpoint built from configuration. */
    tcp_endpoint make_bind_endpoint() const;

  private:
    vix::config::Config &config_;
    std::shared_ptr<io_context> io_context_;
    std::unique_ptr<tcp_listener> listener_;
    std::shared_ptr<vix::router::Router> router_;
    std::shared_ptr<vix::executor::RuntimeExecutor> executor_;
    std::vector<std::thread> io_threads_;
    std::thread metrics_thread_;
    std::atomic<bool> stop_requested_{false};
    std::chrono::steady_clock::time_point startup_t0_{};
    std::atomic<int> bound_port_{0};
  };

} // namespace vix::server

#endif // VIX_HTTP_SERVER_HPP
