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
#include <condition_variable>
#include <mutex>

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
   * @brief Default maximum number of CPU worker threads used by the HTTP server runtime.
   */
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

    /**
     * @brief Stop the server and release all resources.
     */
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

    /**
     * @brief Compute the number of CPU worker threads to use based on configuration and hardware.
     *
     * @return Number of worker threads to launch.
     */
    std::size_t calculate_io_thread_count();

    /**
     * @brief Return the router used to dispatch incoming HTTP requests.
     *
     * @return Shared router instance.
     */
    std::shared_ptr<vix::router::Router> getRouter()
    {
      return router_;
    }

    /**
     * @brief Periodically collect and log runtime metrics.
     */
    void monitor_metrics();

    /**
     * @brief Request an asynchronous server shutdown.
     */
    void stop_async();

    /**
     * @brief Join all internal worker threads.
     */
    void join_threads();

    /**
     * @brief Return true if a stop has been requested.
     *
     * @return true if shutdown was requested, otherwise false.
     */
    bool is_stop_requested() const
    {
      return stop_requested_.load(std::memory_order_acquire);
    }

    /**
     * @brief Stop the server and block until all threads have exited.
     */
    void stop_blocking();

    /**
     * @brief Return the actual TCP port bound by the listener.
     *
     * This is useful when the configured port was 0 (ephemeral port).
     *
     * @return Bound port, or 0 if the server is not bound yet.
     */
    int bound_port() const noexcept
    {
      return bound_port_.load(std::memory_order_relaxed);
    }

  private:
    /**
     * @brief Initialize the native Vix TCP listener on the given port.
     *
     * @param port TCP port to bind.
     */
    task<void> init_listener(unsigned short port);
    task<void> start_server();

    /**
     * @brief Main asynchronous accept loop.
     *
     * Accepts new TCP connections from the listener and dispatches each connection
     * to handle_client().
     *
     * @return Asynchronous task representing the accept loop.
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
     *
     * @param stream Accepted TCP stream.
     * @return Asynchronous task representing the client session.
     */
    task<void> handle_client(std::unique_ptr<tcp_stream> stream);

    /**
     * @brief Close a client stream safely.
     *
     * @param stream Stream to close.
     */
    void close_stream(std::unique_ptr<tcp_stream> stream);

    /**
     * @brief Launch the configured number of CPU worker threads for runtime support.
     */
    void start_io_threads();

    /**
     * @brief Return the bind endpoint built from configuration.
     *
     * @return TCP endpoint to bind.
     */
    tcp_endpoint make_bind_endpoint() const;

    /**
     * @brief Return true when an accept error should be ignored during shutdown.
     *
     * This is used to silence expected shutdown-side errors such as
     * bad file descriptor or operation canceled after the listener is closed.
     *
     * @param e Exception raised by the accept operation.
     * @return true if the error should not be logged, otherwise false.
     */
    bool should_silence_accept_error(const std::exception &e) const noexcept;

    /**
     * @brief Return true if the listener exists and is still open.
     *
     * @return true if the listener is open, otherwise false.
     */
    bool is_listener_open() const noexcept;

  private:
    /**
     * @brief Server configuration reference.
     */
    vix::config::Config &config_;

    /**
     * @brief Native Vix async I/O context.
     */
    std::shared_ptr<io_context> io_context_;

    /**
     * @brief TCP listener used to accept incoming connections.
     */
    std::unique_ptr<tcp_listener> listener_;

    /**
     * @brief Router used to dispatch HTTP requests.
     */
    std::shared_ptr<vix::router::Router> router_;

    /**
     * @brief Runtime executor used for application work.
     */
    std::shared_ptr<vix::executor::RuntimeExecutor> executor_;

    /**
     * @brief Worker threads running the Vix async I/O context.
     */
    std::vector<std::thread> io_threads_;

    /**
     * @brief Background thread used to monitor runtime metrics.
     */
    std::thread metrics_thread_;

    /**
     * @brief Mutex protecting the metrics monitor wait state.
     */
    std::mutex metrics_mutex_;

    /**
     * @brief Condition variable used to wake the metrics thread during shutdown.
     */
    std::condition_variable metrics_cv_;

    /**
     * @brief Protects the join phase against concurrent callers.
     */
    mutable std::mutex join_mutex_;

    /**
     * @brief Guards against running thread joins multiple times.
     */
    std::atomic<bool> threads_joined_{false};

    /**
     * @brief Indicates whether shutdown has been requested.
     */
    std::atomic<bool> stop_requested_{false};

    /**
     * @brief Guards against starting multiple accept loops.
     */
    std::atomic<bool> accept_loop_started_{false};

    /**
     * @brief Actual port currently bound by the listener.
     */
    std::atomic<int> bound_port_{0};

    /**
     * @brief Startup timestamp used for uptime and monitoring purposes.
     */
    std::chrono::steady_clock::time_point startup_t0_{};
  };

} // namespace vix::server

#endif // VIX_HTTP_SERVER_HPP
