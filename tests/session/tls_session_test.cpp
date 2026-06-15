/**
 *
 * @file tls_session_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <vix/async/net/tcp.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>
#include <vix/session/TlsSession.hpp>

namespace
{
  using TlsSession = vix::session::TlsSession;
  using TcpStream = vix::async::net::tcp_stream;
  using Config = vix::config::Config;
  using Router = vix::router::Router;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;

  static void test_tls_session_type_traits()
  {
    static_assert(!std::is_default_constructible_v<TlsSession>);

    static_assert(std::is_constructible_v<
                  TlsSession,
                  std::unique_ptr<TcpStream>,
                  Router &,
                  const Config &,
                  std::shared_ptr<RuntimeExecutor>>);

    static_assert(!std::is_copy_constructible_v<TlsSession>);
    static_assert(!std::is_copy_assignable_v<TlsSession>);

    static_assert(!std::is_move_constructible_v<TlsSession>);
    static_assert(!std::is_move_assignable_v<TlsSession>);

    static_assert(std::is_destructible_v<TlsSession>);
  }

  static void test_tls_session_uses_shared_from_this_base()
  {
    static_assert(std::is_base_of_v<
                  std::enable_shared_from_this<TlsSession>,
                  TlsSession>);
  }

  static void test_null_stream_constructor_throws()
  {
    Router router;
    Config config;

    std::unique_ptr<TcpStream> stream{};
    std::shared_ptr<RuntimeExecutor> executor{};

    bool threw = false;

    try
    {
      TlsSession session{
          std::move(stream),
          router,
          config,
          executor};

      (void)session;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("TlsSession requires a valid TCP stream") != std::string::npos);
    }

    assert(threw);
  }

} // namespace

int main()
{
  test_tls_session_type_traits();
  test_tls_session_uses_shared_from_this_base();
  test_null_stream_constructor_throws();

  return 0;
}
