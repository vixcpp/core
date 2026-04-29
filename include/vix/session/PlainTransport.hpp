/**
 *
 * @file PlainTransport.hpp
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

#ifndef VIX_SESSION_PLAIN_TRANSPORT_HPP
#define VIX_SESSION_PLAIN_TRANSPORT_HPP

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/session/Transport.hpp>

namespace vix::session
{
  using vix::async::core::cancel_token;
  using vix::async::core::task;
  using vix::async::net::tcp_stream;

  /**
   * @brief Plain TCP transport for the HTTP session layer.
   *
   * PlainTransport adapts vix::async::net::tcp_stream to the generic
   * Transport interface. It is used for normal HTTP connections without TLS.
   */
  class PlainTransport final : public Transport
  {
  public:
    /**
     * @brief Create a plain TCP transport from an accepted TCP stream.
     *
     * @param stream Accepted native Vix TCP stream.
     *
     * @throw std::invalid_argument If stream is null.
     */
    explicit PlainTransport(std::unique_ptr<tcp_stream> stream)
        : stream_(std::move(stream))
    {
      if (!stream_)
      {
        throw std::invalid_argument("PlainTransport requires a valid TCP stream");
      }
    }

    PlainTransport(const PlainTransport &) = delete;
    PlainTransport &operator=(const PlainTransport &) = delete;

    PlainTransport(PlainTransport &&) noexcept = delete;
    PlainTransport &operator=(PlainTransport &&) noexcept = delete;

    /**
     * @brief Destroy the plain transport.
     */
    ~PlainTransport() noexcept override = default;

    /**
     * @brief Read bytes from the underlying TCP stream.
     */
    task<std::size_t> async_read(
        std::span<std::byte> buffer,
        cancel_token token) override
    {
      co_return co_await stream_->async_read(buffer, token);
    }

    /**
     * @brief Write bytes to the underlying TCP stream.
     */
    task<std::size_t> async_write(
        std::span<const std::byte> buffer,
        cancel_token token) override
    {
      co_return co_await stream_->async_write(buffer, token);
    }

    /**
     * @brief Return true if the underlying TCP stream is open.
     */
    [[nodiscard]] bool is_open() const noexcept override
    {
      return stream_ && stream_->is_open();
    }

    /**
     * @brief Close the underlying TCP stream.
     */
    void close() noexcept override
    {
      if (!stream_)
      {
        return;
      }

      try
      {
        stream_->close();
      }
      catch (...)
      {
      }
    }

  private:
    std::unique_ptr<tcp_stream> stream_;
  };

} // namespace vix::session

#endif // VIX_SESSION_PLAIN_TRANSPORT_HPP
