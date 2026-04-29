/**
 *
 * @file TlsTransport.hpp
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

#ifndef VIX_SESSION_TLS_TRANSPORT_HPP
#define VIX_SESSION_TLS_TRANSPORT_HPP

#include <cstddef>
#include <memory>
#include <span>
#include <string>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/server/TlsConfig.hpp>
#include <vix/session/Transport.hpp>

namespace vix::session
{
  using vix::async::core::cancel_token;
  using vix::async::core::task;
  using vix::async::net::tcp_stream;

  /**
   * @brief TLS transport for the HTTP session layer.
   *
   * TlsTransport adapts an accepted TCP stream into an encrypted transport.
   * It owns the TCP stream, performs a TLS server handshake, then exposes
   * encrypted read/write operations through the generic Transport interface.
   *
   * The HTTP session parser remains independent from TLS and only talks to
   * Transport.
   */
  class TlsTransport final : public Transport
  {
  public:
    /**
     * @brief Create a TLS transport from an accepted TCP stream and TLS config.
     *
     * @param stream Accepted native Vix TCP stream.
     * @param config TLS configuration containing certificate and key paths.
     *
     * @throw std::invalid_argument If the stream is null or TLS config is invalid.
     * @throw std::runtime_error If TLS context creation fails.
     */
    explicit TlsTransport(
        std::unique_ptr<tcp_stream> stream,
        vix::server::TlsConfig config);

    TlsTransport(const TlsTransport &) = delete;
    TlsTransport &operator=(const TlsTransport &) = delete;

    TlsTransport(TlsTransport &&) noexcept = delete;
    TlsTransport &operator=(TlsTransport &&) noexcept = delete;

    /**
     * @brief Destroy the TLS transport and release TLS resources.
     */
    ~TlsTransport() noexcept override;

    /**
     * @brief Perform the TLS server handshake.
     *
     * Must be called before passing this transport to Session.
     */
    task<void> async_handshake(cancel_token token = {});

    /**
     * @brief Read decrypted bytes from the TLS connection.
     */
    task<std::size_t> async_read(
        std::span<std::byte> buffer,
        cancel_token token) override;

    /**
     * @brief Write plaintext bytes through the TLS connection.
     */
    task<std::size_t> async_write(
        std::span<const std::byte> buffer,
        cancel_token token) override;

    /**
     * @brief Return true if the underlying TCP stream is open.
     */
    [[nodiscard]] bool is_open() const noexcept override;

    /**
     * @brief Close the TLS transport and the underlying TCP stream.
     */
    void close() noexcept override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
  };

} // namespace vix::session

#endif // VIX_SESSION_TLS_TRANSPORT_HPP
