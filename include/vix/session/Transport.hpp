/**
 *
 * @file Transport.hpp
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

#ifndef VIX_SESSION_TRANSPORT_HPP
#define VIX_SESSION_TRANSPORT_HPP

#include <cstddef>
#include <span>
#include <string_view>
#include <system_error>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/task.hpp>

namespace vix::session
{
  using vix::async::core::cancel_token;
  using vix::async::core::task;

  /**
   * @brief Abstract transport used by the HTTP session layer.
   *
   * The HTTP parser and response writer should not know whether bytes are
   * transported over plain TCP or TLS.
   *
   * Implementations are responsible for:
   * - reading encrypted or plain bytes
   * - writing encrypted or plain bytes
   * - reporting whether the connection is still open
   * - closing the underlying connection
   */
  class Transport
  {
  public:
    /**
     * @brief Construct a transport.
     */
    Transport() noexcept = default;

    /**
     * @brief Destroy the transport.
     */
    virtual ~Transport() noexcept = default;

    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;

    Transport(Transport &&) noexcept = delete;
    Transport &operator=(Transport &&) noexcept = delete;

    /**
     * @brief Read bytes from the transport.
     *
     * @param buffer Destination buffer.
     * @param token Cancellation token.
     *
     * @return Number of bytes read.
     */
    virtual task<std::size_t> async_read(
        std::span<std::byte> buffer,
        cancel_token token) = 0;

    /**
     * @brief Write bytes to the transport.
     *
     * @param buffer Source buffer.
     * @param token Cancellation token.
     *
     * @return Number of bytes written.
     */
    virtual task<std::size_t> async_write(
        std::span<const std::byte> buffer,
        cancel_token token) = 0;

    /**
     * @brief Return true if the transport is currently open.
     */
    [[nodiscard]] virtual bool is_open() const noexcept = 0;

    /**
     * @brief Close the transport.
     *
     * Implementations should make this operation best-effort and idempotent.
     */
    virtual void close() noexcept = 0;
  };

} // namespace vix::session

#endif // VIX_SESSION_TRANSPORT_HPP
