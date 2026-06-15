/**
 *
 * @file tls_transport_test.cpp
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
#include <vix/server/TlsConfig.hpp>
#include <vix/session/TlsTransport.hpp>
#include <vix/session/Transport.hpp>

namespace
{
  using TlsTransport = vix::session::TlsTransport;
  using Transport = vix::session::Transport;
  using TcpStream = vix::async::net::tcp_stream;
  using TlsConfig = vix::server::TlsConfig;

  static void test_tls_transport_type_traits()
  {
    static_assert(std::is_final_v<TlsTransport>);
    static_assert(std::is_base_of_v<Transport, TlsTransport>);

    static_assert(!std::is_default_constructible_v<TlsTransport>);

    static_assert(std::is_constructible_v<
                  TlsTransport,
                  std::unique_ptr<TcpStream>,
                  TlsConfig>);

    static_assert(!std::is_copy_constructible_v<TlsTransport>);
    static_assert(!std::is_copy_assignable_v<TlsTransport>);

    static_assert(!std::is_move_constructible_v<TlsTransport>);
    static_assert(!std::is_move_assignable_v<TlsTransport>);

    static_assert(std::is_destructible_v<TlsTransport>);
    static_assert(std::has_virtual_destructor_v<Transport>);
  }

  static void test_tls_config_is_usable_as_constructor_argument()
  {
    TlsConfig config;

    (void)config;

    static_assert(std::is_default_constructible_v<TlsConfig>);
    static_assert(std::is_copy_constructible_v<TlsConfig>);
    static_assert(std::is_move_constructible_v<TlsConfig>);
  }

  static void test_null_stream_constructor_throws_invalid_argument()
  {
    std::unique_ptr<TcpStream> stream{};
    TlsConfig config{};

    bool threw = false;

    try
    {
      TlsTransport transport{
          std::move(stream),
          config};

      (void)transport;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("TlsTransport requires a valid TCP stream") != std::string::npos);
    }

    assert(threw);
  }

  static void test_tls_transport_can_be_referenced_as_transport_type()
  {
    static_assert(std::is_convertible_v<TlsTransport *, Transport *>);
    static_assert(std::is_convertible_v<const TlsTransport *, const Transport *>);
  }

} // namespace

int main()
{
  test_tls_transport_type_traits();
  test_tls_config_is_usable_as_constructor_argument();
  test_null_stream_constructor_throws_invalid_argument();
  test_tls_transport_can_be_referenced_as_transport_type();

  return 0;
}
