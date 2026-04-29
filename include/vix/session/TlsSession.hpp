/**
 *
 * @file TlsSession.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_TLS_SESSION_HPP
#define VIX_TLS_SESSION_HPP

#include <memory>

#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>

namespace vix::session
{
  using vix::async::core::task;
  using vix::async::net::tcp_stream;

  /**
   * @brief HTTPS client connection session.
   *
   * TlsSession owns an already accepted TCP stream.
   * It performs the TLS server handshake first, then processes HTTP requests
   * over the encrypted connection.
   *
   * This class is intentionally separate from Session so plain HTTP remains
   * unchanged and TLS stays fully optional.
   */
  class TlsSession : public std::enable_shared_from_this<TlsSession>
  {
  public:
    /**
     * @brief Create a TLS session from an accepted TCP stream.
     *
     * @param stream Accepted native Vix TCP stream.
     * @param router Router used to dispatch HTTP requests.
     * @param config Server configuration.
     * @param executor Executor used for handler execution.
     *
     * @throw std::invalid_argument If stream or executor is invalid.
     */
    explicit TlsSession(
        std::unique_ptr<tcp_stream> stream,
        vix::router::Router &router,
        const vix::config::Config &config,
        std::shared_ptr<vix::executor::RuntimeExecutor> executor);

    TlsSession(const TlsSession &) = delete;
    TlsSession &operator=(const TlsSession &) = delete;

    TlsSession(TlsSession &&) = delete;
    TlsSession &operator=(TlsSession &&) = delete;

    /**
     * @brief Destroy the TLS session.
     */
    ~TlsSession();

    /**
     * @brief Start the TLS handshake and HTTP session lifecycle.
     */
    task<void> run();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
  };

} // namespace vix::session

#endif // VIX_TLS_SESSION_HPP
