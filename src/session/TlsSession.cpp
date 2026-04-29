/**
 *
 * @file TlsSession.cpp
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

#include <vix/session/TlsSession.hpp>

#include <memory>
#include <stdexcept>
#include <utility>
#include <string>

#include <vix/server/TlsConfig.hpp>
#include <vix/session/Session.hpp>
#include <vix/session/TlsTransport.hpp>
#include <vix/utils/Logger.hpp>

namespace vix::session
{
  using Logger = vix::utils::Logger;

  namespace
  {
    inline Logger &log()
    {
      return Logger::getInstance();
    }
  } // namespace

  struct TlsSession::Impl
  {
    std::unique_ptr<tcp_stream> stream;
    vix::router::Router &router;
    const vix::config::Config &config;
    std::shared_ptr<vix::executor::RuntimeExecutor> executor;

    Impl(
        std::unique_ptr<tcp_stream> input_stream,
        vix::router::Router &input_router,
        const vix::config::Config &input_config,
        std::shared_ptr<vix::executor::RuntimeExecutor> input_executor)
        : stream(std::move(input_stream)),
          router(input_router),
          config(input_config),
          executor(std::move(input_executor))
    {
      if (!stream)
      {
        throw std::invalid_argument("TlsSession requires a valid TCP stream");
      }

      if (!executor)
      {
        throw std::invalid_argument("TlsSession requires a valid executor");
      }
    }

    void close_stream_noexcept() noexcept
    {
      if (!stream)
      {
        return;
      }

      try
      {
        stream->close();
      }
      catch (...)
      {
      }
    }
  };

  TlsSession::TlsSession(
      std::unique_ptr<tcp_stream> stream,
      vix::router::Router &router,
      const vix::config::Config &config,
      std::shared_ptr<vix::executor::RuntimeExecutor> executor)
      : impl_(std::make_unique<Impl>(
            std::move(stream),
            router,
            config,
            std::move(executor)))
  {
  }

  TlsSession::~TlsSession() = default;

  task<void> TlsSession::run()
  {
    if (!impl_)
    {
      co_return;
    }

    const vix::server::TlsConfig tls = impl_->config.getTlsConfig();

    if (!tls.is_enabled())
    {
      log().log(
          Logger::Level::Warn,
          "[tls] TlsSession started while TLS is disabled");

      impl_->close_stream_noexcept();
      co_return;
    }

    if (!tls.is_configured())
    {
      log().log(
          Logger::Level::Error,
          "[tls] TLS is enabled but certificate or private key file is missing");

      impl_->close_stream_noexcept();
      co_return;
    }

    try
    {
      auto transport = std::make_unique<TlsTransport>(
          std::move(impl_->stream),
          tls);

      co_await transport->async_handshake();

      auto session = std::make_shared<Session>(
          std::move(transport),
          impl_->router,
          impl_->config,
          impl_->executor);

      co_await session->run();
    }
    catch (const std::exception &e)
    {
      const std::string msg = e.what();

      if (msg.find("unexpected eof while reading") != std::string::npos ||
          msg.find("connection reset") != std::string::npos ||
          msg.find("broken pipe") != std::string::npos)
      {
        log().log(
            Logger::Level::Debug,
            "[tls] client closed connection during handshake: {}",
            e.what());
      }
      else
      {
        log().log(
            Logger::Level::Error,
            "[tls] TLS session failed: {}",
            e.what());
      }

      impl_->close_stream_noexcept();
    }

    co_return;
  }

} // namespace vix::session
