/**
 *
 * @file HTTPServer.hpp
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
#ifndef VIX_HTTP_SERVER_HPP
#define VIX_HTTP_SERVER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
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

  /**
   * @brief Default maximum number of I/O worker threads used by the HTTP server.
   *
   * This value is used when configuration does not provide an explicit thread
   * count and hardware concurrency cannot be detected reliably.
   */
  constexpr std::size_t NUMBER_OF_THREADS = 8;

  /**
   * @brief Native asynchronous HTTP server built on top of Vix async.
   *
   * Responsibilities:
   * - own the native async I/O context
   * - bind and manage the TCP listener
   * - accept incoming connections
   * - dispatch each client session through the router
   * - delegate application work through a generic executor
   * - manage monitoring and graceful shutdown
   *
   * This server does not depend on Boost.Asio or Boost.Beast.
   * Networking is provided by vix::async::net and application execution is
   */
  class HTTPServer
  {
  public:
    /**
     * @brief Construct an HTTP server from configuration and executor.
     *
     * @param config Server configuration.
     * @param executor Shared executor used for application work.
     *
     * @throw std::invalid_argument If the executor pointer is null.
     */
    explicit HTTPServer(
        vix::config::Config &config,
        std::shared_ptr<vix::executor::RuntimeExecutor> executor);

    HTTPServer(const HTTPServer &) = delete;
    HTTPServer &operator=(const HTTPServer &) = delete;

    HTTPServer(HTTPServer &&) = delete;
    HTTPServer &operator=(HTTPServer &&) = delete;

    /**
     * @brief Destroy the server and stop all owned resources safely.
     *
     * Destruction performs a best-effort asynchronous shutdown and joins all
     * internal threads when needed.
     */
    ~HTTPServer();

    /**
     * @brief Start the server runtime and begin accepting TCP connections.
     *
     * Typical startup flow:
     * - validate configuration
     * - launch I/O threads
     * - initialize the native listener
     * - start the accept loop
     * - start background metrics monitoring
     *
     * This function throws on fatal startup failure.
     */
    void run();

    /**
     * @brief Start the asynchronous accept loop.
     *
     * This method is public for API compatibility with previous versions.
     * If the accept loop is already running, the call has no effect.
     */
    void start_accept();

    /**
     * @brief Compute the number of I/O threads to launch.
     *
     * Resolution order:
     * - explicit value from configuration if present
     * - hardware concurrency if available
     * - fallback to @ref NUMBER_OF_THREADS
     *
     * @return Number of I/O threads to launch.
     */
    [[nodiscard]] std::size_t calculate_io_thread_count();

    /**
     * @brief Return the router used to dispatch HTTP requests.
     *
     * @return Shared router instance.
     */
    [[nodiscard]] std::shared_ptr<vix::router::Router> getRouter() const noexcept
    {
      return router_;
    }

    /**
     * @brief Start the background monitoring loop.
     *
     * The monitoring thread periodically wakes up while the server is running.
     * It can be extended later to expose metrics, heartbeats, or health logs.
     */
    void monitor_metrics();

    /**
     * @brief Request an asynchronous server shutdown.
     *
     * This closes the listener, stops the I/O context, and wakes background
     * monitor threads. The function does not join worker threads by itself.
     */
    void stop_async();

    /**
     * @brief Join all internal threads owned by the server.
     *
     * This function is idempotent and safe against repeated calls.
     */
    void join_threads();

    /**
     * @brief Return whether shutdown has been requested.
     *
     * @return true if a stop was requested, otherwise false.
     */
    [[nodiscard]] bool is_stop_requested() const noexcept
    {
      return stop_requested_.load(std::memory_order_acquire);
    }

    /**
     * @brief Stop the server and block until all internal threads have exited.
     *
     * This is the strong shutdown path:
     * - request stop
     * - stop the async context
     * - close the listener
     * - join all internal threads
     */
    void stop_blocking();

    /**
     * @brief Return the actual TCP port currently bound by the listener.
     *
     * This is especially useful when configuration requested port 0
     * and the operating system selected an ephemeral port.
     *
     * @return Bound TCP port, or 0 if not bound yet.
     */
    [[nodiscard]] int bound_port() const noexcept
    {
      return bound_port_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Return the shared executor used by the server.
     *
     * @return Shared executor instance.
     */
    std::shared_ptr<vix::executor::RuntimeExecutor> executor() const noexcept
    {
      return executor_;
    }

  private:
    /**
     * @brief Initialize the native TCP listener on the requested port.
     *
     * @param port TCP port to bind.
     *
     * @return Asynchronous task completing once the listener is ready.
     */
    task<void> init_listener(unsigned short port);

    /**
     * @brief Internal asynchronous startup routine.
     *
     * This coroutine ensures the listener is initialized and then starts the
     * accept loop if shutdown has not already been requested.
     *
     * @return Asynchronous startup task.
     */
    task<void> start_server();

    /**
     * @brief Main asynchronous accept loop.
     *
     * The loop repeatedly accepts incoming TCP streams and dispatches each
     * accepted stream to @ref handle_client.
     *
     * @return Asynchronous accept loop task.
     */
    task<void> accept_loop();

    /**
     * @brief Handle one accepted client connection.
     *
     * The implementation is responsible for creating the HTTP session wrapper,
     * running request parsing and routing, and ensuring the stream is closed
     * on failure when necessary.
     *
     * @param stream Accepted TCP stream.
     *
     * @return Asynchronous client handling task.
     */
    task<void> handle_client(std::unique_ptr<tcp_stream> stream);

    /**
     * @brief Close a client stream safely.
     *
     * Any exception raised while closing the stream is ignored.
     *
     * @param stream Stream to close.
     */
    void close_stream(std::unique_ptr<tcp_stream> stream);

    /**
     * @brief Launch the configured number of I/O threads.
     *
     * Each thread runs the shared native async I/O context.
     */
    void start_io_threads();

    /**
     * @brief Build the bind endpoint from the current server configuration.
     *
     * @return TCP endpoint used for listener binding.
     */
    [[nodiscard]] tcp_endpoint make_bind_endpoint() const;

    /**
     * @brief Return true when an accept-side exception should be silenced.
     *
     * Some listener errors are expected during shutdown, such as:
     * - operation canceled
     * - bad file descriptor
     * - native cancellation errors after listener close
     *
     * @param e Exception raised by the accept operation.
     *
     * @return true if the error should not be logged.
     */
    [[nodiscard]] bool should_silence_accept_error(const std::exception &e) const noexcept;

    /**
     * @brief Return true if the listener exists and is open.
     *
     * @return true if the listener is present and open.
     */
    [[nodiscard]] bool is_listener_open() const noexcept;

  private:
    /**
     * @brief Server configuration reference.
     */
    vix::config::Config &config_;

    /**
     * @brief Shared native Vix async I/O context.
     */
    std::shared_ptr<io_context> io_context_;

    /**
     * @brief TCP listener accepting incoming client connections.
     */
    std::unique_ptr<tcp_listener> listener_;

    /**
     * @brief Router used to dispatch incoming HTTP requests.
     */
    std::shared_ptr<vix::router::Router> router_;

    /**
     * @brief Generic executor used for application work.
     */
    std::shared_ptr<vix::executor::RuntimeExecutor> executor_;

    /**
     * @brief Threads running the native async I/O context.
     */
    std::vector<std::thread> io_threads_;

    /**
     * @brief Background thread used for periodic monitoring.
     */
    std::thread metrics_thread_;

    /**
     * @brief Mutex protecting the monitor wait state.
     */
    std::mutex metrics_mutex_;

    /**
     * @brief Condition variable used to wake the monitoring thread.
     */
    std::condition_variable metrics_cv_;

    /**
     * @brief Mutex protecting the thread join phase.
     */
    mutable std::mutex join_mutex_;

    /**
     * @brief Indicates whether internal threads were already joined.
     */
    std::atomic<bool> threads_joined_;

    /**
     * @brief Indicates whether shutdown has been requested.
     */
    std::atomic<bool> stop_requested_;

    /**
     * @brief Guards against starting the accept loop more than once.
     */
    std::atomic<bool> accept_loop_started_;

    /**
     * @brief Actual port bound by the listener.
     */
    std::atomic<int> bound_port_;

    /**
     * @brief Startup timestamp used for uptime and monitoring purposes.
     */
    std::chrono::steady_clock::time_point startup_t0_;
  };

} // namespace vix::server

#endif // VIX_HTTP_SERVER_HPP
