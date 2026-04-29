/**
 *
 * @file TlsTransport.cpp
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

#include <vix/session/TlsTransport.hpp>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#if VIX_CORE_HAS_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

namespace vix::session
{

#if !VIX_CORE_HAS_OPENSSL

  struct TlsTransport::Impl
  {
    std::unique_ptr<tcp_stream> stream;

    explicit Impl(std::unique_ptr<tcp_stream> input_stream)
        : stream(std::move(input_stream))
    {
      if (!stream)
      {
        throw std::invalid_argument("TlsTransport requires a valid TCP stream");
      }
    }
  };

  TlsTransport::TlsTransport(
      std::unique_ptr<tcp_stream> stream,
      vix::server::TlsConfig)
      : impl_(std::make_unique<Impl>(std::move(stream)))
  {
    throw std::runtime_error(
        "TLS support is not available because Vix core was built without OpenSSL");
  }

  TlsTransport::~TlsTransport() noexcept = default;

  task<void> TlsTransport::async_handshake(cancel_token)
  {
    throw std::runtime_error(
        "TLS handshake is not available because Vix core was built without OpenSSL");
  }

  task<std::size_t> TlsTransport::async_read(
      std::span<std::byte>,
      cancel_token)
  {
    throw std::runtime_error(
        "TLS read is not available because Vix core was built without OpenSSL");
  }

  task<std::size_t> TlsTransport::async_write(
      std::span<const std::byte>,
      cancel_token)
  {
    throw std::runtime_error(
        "TLS write is not available because Vix core was built without OpenSSL");
  }

  bool TlsTransport::is_open() const noexcept
  {
    return false;
  }

  void TlsTransport::close() noexcept
  {
  }

#else

  namespace
  {
    [[nodiscard]] std::string openssl_error_string()
    {
      unsigned long err = ERR_get_error();

      if (err == 0)
      {
        return "unknown OpenSSL error";
      }

      char buffer[256]{};
      ERR_error_string_n(err, buffer, sizeof(buffer));

      return std::string(buffer);
    }

    [[nodiscard]] std::runtime_error make_ssl_error(const std::string &message)
    {
      return std::runtime_error(message + ": " + openssl_error_string());
    }

    void ensure_openssl_initialized()
    {
      static const bool initialized = []
      {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        return true;
      }();

      (void)initialized;
    }

    void throw_if_cancelled(vix::async::core::cancel_token token)
    {
      if (token.is_cancelled())
      {
        throw std::system_error(vix::async::core::cancelled_ec());
      }
    }
  } // namespace

  struct TlsTransport::Impl
  {
    std::unique_ptr<tcp_stream> stream{};
    vix::server::TlsConfig config{};

    SSL_CTX *ctx{nullptr};
    SSL *ssl{nullptr};

    bool handshake_done{false};
    bool closed{false};

    Impl(
        std::unique_ptr<tcp_stream> input_stream,
        vix::server::TlsConfig input_config)
        : stream(std::move(input_stream)),
          config(std::move(input_config))
    {
      if (!stream)
      {
        throw std::invalid_argument("TlsTransport requires a valid TCP stream");
      }

      if (!config.is_valid())
      {
        throw std::invalid_argument(
            "TlsTransport requires enabled TLS with certificate and private key files");
      }

      ensure_openssl_initialized();

      ctx = SSL_CTX_new(TLS_server_method());
      if (!ctx)
      {
        throw make_ssl_error("failed to create TLS context");
      }

      SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

      if (SSL_CTX_use_certificate_chain_file(ctx, config.cert_file.c_str()) != 1)
      {
        throw make_ssl_error("failed to load TLS certificate file");
      }

      if (SSL_CTX_use_PrivateKey_file(ctx, config.key_file.c_str(), SSL_FILETYPE_PEM) != 1)
      {
        throw make_ssl_error("failed to load TLS private key file");
      }

      if (SSL_CTX_check_private_key(ctx) != 1)
      {
        throw make_ssl_error("TLS private key does not match certificate");
      }

      ssl = SSL_new(ctx);
      if (!ssl)
      {
        throw make_ssl_error("failed to create TLS connection state");
      }

      const int fd = stream->native_handle();
      if (fd < 0)
      {
        throw std::runtime_error("invalid native socket handle for TLS");
      }

      if (SSL_set_fd(ssl, fd) != 1)
      {
        throw make_ssl_error("failed to attach socket to TLS connection");
      }

      SSL_set_accept_state(ssl);
    }

    ~Impl() noexcept
    {
      close_noexcept();

      if (ssl)
      {
        SSL_free(ssl);
        ssl = nullptr;
      }

      if (ctx)
      {
        SSL_CTX_free(ctx);
        ctx = nullptr;
      }
    }

    void close_noexcept() noexcept
    {
      if (closed)
      {
        return;
      }

      closed = true;

      if (ssl && handshake_done)
      {
        try
        {
          SSL_shutdown(ssl);
        }
        catch (...)
        {
        }
      }

      if (stream)
      {
        try
        {
          stream->close();
        }
        catch (...)
        {
        }
      }
    }

    [[nodiscard]] bool open() const noexcept
    {
      return !closed && stream && stream->is_open();
    }
  };

  TlsTransport::TlsTransport(
      std::unique_ptr<tcp_stream> stream,
      vix::server::TlsConfig config)
      : impl_(std::make_unique<Impl>(std::move(stream), std::move(config)))
  {
  }

  TlsTransport::~TlsTransport() noexcept = default;

  task<void> TlsTransport::async_handshake(cancel_token token)
  {
    if (!impl_ || !impl_->ssl)
    {
      throw std::runtime_error("TlsTransport is not initialized");
    }

    throw_if_cancelled(token);

    const int rc = SSL_accept(impl_->ssl);
    if (rc != 1)
    {
      const int ssl_error = SSL_get_error(impl_->ssl, rc);

      if (ssl_error == SSL_ERROR_SYSCALL && errno != 0)
      {
        throw std::system_error(errno, std::generic_category(), "TLS handshake syscall failed");
      }

      throw make_ssl_error("TLS handshake failed");
    }

    impl_->handshake_done = true;
    co_return;
  }

  task<std::size_t> TlsTransport::async_read(
      std::span<std::byte> buffer,
      cancel_token token)
  {
    if (!impl_ || !impl_->open())
    {
      co_return 0;
    }

    if (!impl_->handshake_done)
    {
      throw std::runtime_error("TLS read attempted before handshake");
    }

    throw_if_cancelled(token);

    if (buffer.empty())
    {
      co_return 0;
    }

    const int n = SSL_read(
        impl_->ssl,
        buffer.data(),
        static_cast<int>(buffer.size()));

    if (n > 0)
    {
      co_return static_cast<std::size_t>(n);
    }

    const int ssl_error = SSL_get_error(impl_->ssl, n);

    if (ssl_error == SSL_ERROR_ZERO_RETURN)
    {
      co_return 0;
    }

    if (ssl_error == SSL_ERROR_SYSCALL && errno != 0)
    {
      throw std::system_error(errno, std::generic_category(), "TLS read syscall failed");
    }

    throw make_ssl_error("TLS read failed");
  }

  task<std::size_t> TlsTransport::async_write(
      std::span<const std::byte> buffer,
      cancel_token token)
  {
    if (!impl_ || !impl_->open())
    {
      co_return 0;
    }

    if (!impl_->handshake_done)
    {
      throw std::runtime_error("TLS write attempted before handshake");
    }

    throw_if_cancelled(token);

    if (buffer.empty())
    {
      co_return 0;
    }

    const int n = SSL_write(
        impl_->ssl,
        buffer.data(),
        static_cast<int>(buffer.size()));

    if (n > 0)
    {
      co_return static_cast<std::size_t>(n);
    }

    const int ssl_error = SSL_get_error(impl_->ssl, n);

    if (ssl_error == SSL_ERROR_ZERO_RETURN)
    {
      co_return 0;
    }

    if (ssl_error == SSL_ERROR_SYSCALL && errno != 0)
    {
      throw std::system_error(errno, std::generic_category(), "TLS write syscall failed");
    }

    throw make_ssl_error("TLS write failed");
  }

  bool TlsTransport::is_open() const noexcept
  {
    return impl_ && impl_->open();
  }

  void TlsTransport::close() noexcept
  {
    if (!impl_)
    {
      return;
    }

    impl_->close_noexcept();
  }

#endif // !VIX_CORE_HAS_OPENSSL

} // namespace vix::session
