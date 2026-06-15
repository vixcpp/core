/**
 *
 * @file session_constructor_test.cpp
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
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>
#include <vix/session/Session.hpp>
#include <vix/session/Transport.hpp>

namespace
{
  using Session = vix::session::Session;
  using Transport = vix::session::Transport;
  using TcpStream = vix::async::net::tcp_stream;
  using Config = vix::config::Config;
  using Router = vix::router::Router;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;

  class FakeTransport final : public Transport
  {
  public:
    FakeTransport() = default;

    explicit FakeTransport(bool open)
        : open_(open)
    {
    }

    vix::async::core::task<std::size_t> async_read(
        std::span<std::byte> buffer,
        vix::async::core::cancel_token token) override
    {
      (void)buffer;
      (void)token;

      co_return 0;
    }

    vix::async::core::task<std::size_t> async_write(
        std::span<const std::byte> buffer,
        vix::async::core::cancel_token token) override
    {
      (void)buffer;
      (void)token;

      co_return 0;
    }

    [[nodiscard]] bool is_open() const noexcept override
    {
      return open_;
    }

    void close() noexcept override
    {
      open_ = false;
      ++close_count_;
    }

    [[nodiscard]] int close_count() const noexcept
    {
      return close_count_;
    }

  private:
    bool open_{true};
    int close_count_{0};
  };

  static std::unique_ptr<Transport> make_transport(bool open = true)
  {
    return std::make_unique<FakeTransport>(open);
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
  }

  static void test_session_type_traits()
  {
    static_assert(!std::is_default_constructible_v<Session>);

    static_assert(std::is_constructible_v<
                  Session,
                  std::unique_ptr<Transport>,
                  Router &,
                  const Config &,
                  std::shared_ptr<RuntimeExecutor>>);

    static_assert(std::is_constructible_v<
                  Session,
                  std::unique_ptr<TcpStream>,
                  Router &,
                  const Config &,
                  std::shared_ptr<RuntimeExecutor>>);

    static_assert(!std::is_copy_constructible_v<Session>);
    static_assert(!std::is_copy_assignable_v<Session>);
    static_assert(!std::is_move_constructible_v<Session>);
    static_assert(!std::is_move_assignable_v<Session>);
  }

  static void test_null_transport_constructor_throws()
  {
    Router router;
    Config config;
    auto executor = make_executor();

    std::unique_ptr<Transport> transport{};

    bool threw = false;

    try
    {
      Session session{
          std::move(transport),
          router,
          config,
          executor};

      (void)session;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("Session requires a valid transport") != std::string::npos);
    }

    assert(threw);

    executor->stop();
  }

  static void test_null_executor_constructor_throws()
  {
    Router router;
    Config config;

    std::shared_ptr<RuntimeExecutor> executor{};

    bool threw = false;

    try
    {
      Session session{
          make_transport(),
          router,
          config,
          executor};

      (void)session;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("Session requires a valid executor") != std::string::npos);
    }

    assert(threw);
  }

  static void test_valid_transport_and_executor_constructs()
  {
    Router router;
    Config config;
    auto executor = make_executor();

    bool constructed = false;

    try
    {
      Session session{
          make_transport(),
          router,
          config,
          executor};

      (void)session;
      constructed = true;
    }
    catch (...)
    {
      constructed = false;
    }

    assert(constructed);

    executor->stop();
  }

  static void test_closed_transport_still_constructs()
  {
    Router router;
    Config config;
    auto executor = make_executor();

    bool constructed = false;

    try
    {
      Session session{
          make_transport(false),
          router,
          config,
          executor};

      (void)session;
      constructed = true;
    }
    catch (...)
    {
      constructed = false;
    }

    assert(constructed);

    executor->stop();
  }

  static void test_shared_session_construction()
  {
    Router router;
    Config config;
    auto executor = make_executor();

    auto session = std::make_shared<Session>(
        make_transport(),
        router,
        config,
        executor);

    assert(session != nullptr);

    executor->stop();
  }

  static void test_multiple_sessions_can_share_router_config_and_executor()
  {
    Router router;
    Config config;
    auto executor = make_executor();

    auto first = std::make_shared<Session>(
        make_transport(),
        router,
        config,
        executor);

    auto second = std::make_shared<Session>(
        make_transport(),
        router,
        config,
        executor);

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first != second);

    executor->stop();
  }

} // namespace

int main()
{
  test_session_type_traits();

  test_null_transport_constructor_throws();
  test_null_executor_constructor_throws();

  test_valid_transport_and_executor_constructs();
  test_closed_transport_still_constructs();

  test_shared_session_construction();
  test_multiple_sessions_can_share_router_config_and_executor();

  return 0;
}
