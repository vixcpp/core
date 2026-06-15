/**
 *
 * @file plain_transport_test.cpp
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

#include <vix/session/PlainTransport.hpp>
#include <vix/session/Transport.hpp>

namespace
{
  using PlainTransport = vix::session::PlainTransport;
  using Transport = vix::session::Transport;
  using TcpStream = vix::async::net::tcp_stream;

  static void test_plain_transport_type_traits()
  {
    static_assert(std::is_final_v<PlainTransport>);
    static_assert(std::is_base_of_v<Transport, PlainTransport>);

    static_assert(!std::is_default_constructible_v<PlainTransport>);
    static_assert(std::is_constructible_v<PlainTransport, std::unique_ptr<TcpStream>>);

    static_assert(!std::is_copy_constructible_v<PlainTransport>);
    static_assert(!std::is_copy_assignable_v<PlainTransport>);

    static_assert(!std::is_move_constructible_v<PlainTransport>);
    static_assert(!std::is_move_assignable_v<PlainTransport>);

    static_assert(std::has_virtual_destructor_v<Transport>);
  }

  static void test_null_stream_constructor_throws()
  {
    std::unique_ptr<TcpStream> stream{};

    bool threw = false;

    try
    {
      PlainTransport transport{std::move(stream)};
      (void)transport;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();
      assert(message.find("PlainTransport requires a valid TCP stream") != std::string::npos);
    }

    assert(threw);
  }

} // namespace

int main()
{
  test_plain_transport_type_traits();
  test_null_stream_constructor_throws();

  return 0;
}
