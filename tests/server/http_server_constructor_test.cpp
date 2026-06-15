/**
 *
 * @file http_server_constructor_test.cpp
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
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>
#include <vix/server/HTTPServer.hpp>

namespace
{
  using Config = vix::config::Config;
  using HTTPServer = vix::server::HTTPServer;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;

  static void set_env_var(const char *name, const std::string &value)
  {
#if defined(_WIN32)
    const std::string assignment = std::string{name} + "=" + value;
    const int rc = _putenv(assignment.c_str());
    assert(rc == 0);
#else
    const int rc = setenv(name, value.c_str(), 1);
    assert(rc == 0);
#endif
  }

  static Config make_config()
  {
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("SERVER_PORT", "18090");
    set_env_var("SERVER_IO_THREADS", "1");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("LOGGING_ASYNC", "false");

    Config config{};

    assert(config.getServerPort() == 18090);
    assert(config.getIOThreads() == 1);
    assert(config.isTlsEnabled() == false);

    return config;
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
  }

  static void test_http_server_type_traits()
  {
    static_assert(!std::is_default_constructible_v<HTTPServer>);

    static_assert(std::is_constructible_v<
                  HTTPServer,
                  Config &,
                  std::shared_ptr<RuntimeExecutor>>);

    static_assert(!std::is_copy_constructible_v<HTTPServer>);
    static_assert(!std::is_copy_assignable_v<HTTPServer>);

    static_assert(!std::is_move_constructible_v<HTTPServer>);
    static_assert(!std::is_move_assignable_v<HTTPServer>);

    static_assert(std::is_destructible_v<HTTPServer>);
  }

  static void test_constructor_rejects_null_executor()
  {
    Config config = make_config();

    std::shared_ptr<RuntimeExecutor> executor{};

    bool threw = false;

    try
    {
      HTTPServer server{
          config,
          executor};

      (void)server;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("HTTPServer requires a valid executor") != std::string::npos);
    }

    assert(threw);
  }

  static void test_constructor_accepts_valid_executor()
  {
    Config config = make_config();
    auto executor = make_executor();

    bool constructed = false;

    try
    {
      HTTPServer server{
          config,
          executor};

      (void)server;

      constructed = true;
    }
    catch (...)
    {
      constructed = false;
    }

    assert(constructed);

    executor->stop();
  }

  static void test_initial_state_after_construction()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    assert(server.getRouter() != nullptr);
    assert(server.executor() == executor);

    assert(server.is_stop_requested() == false);
    assert(server.bound_port() == 0);

    executor->stop();
  }

  static void test_router_is_created_for_each_server()
  {
    Config first_config = make_config();
    auto first_executor = make_executor();

    HTTPServer first{
        first_config,
        first_executor};

    Config second_config = make_config();
    auto second_executor = make_executor();

    HTTPServer second{
        second_config,
        second_executor};

    assert(first.getRouter() != nullptr);
    assert(second.getRouter() != nullptr);
    assert(first.getRouter() != second.getRouter());

    first_executor->stop();
    second_executor->stop();
  }

  static void test_executor_accessor_returns_original_shared_executor()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    assert(server.executor() == executor);
    assert(server.executor().get() == executor.get());

    executor->stop();
  }

  static void test_server_can_be_constructed_with_different_valid_configs()
  {
    {
      set_env_var("VIX_ENV_SILENT", "true");
      set_env_var("SERVER_PORT", "18091");
      set_env_var("SERVER_IO_THREADS", "1");
      set_env_var("SERVER_TLS_ENABLED", "false");
      set_env_var("LOGGING_ASYNC", "false");

      Config config{};
      auto executor = make_executor();

      HTTPServer server{
          config,
          executor};

      assert(config.getServerPort() == 18091);
      assert(server.getRouter() != nullptr);
      assert(server.executor() == executor);
      assert(!server.is_stop_requested());

      executor->stop();
    }

    {
      set_env_var("VIX_ENV_SILENT", "true");
      set_env_var("SERVER_PORT", "18092");
      set_env_var("SERVER_IO_THREADS", "2");
      set_env_var("SERVER_TLS_ENABLED", "false");
      set_env_var("LOGGING_ASYNC", "false");

      Config config{};
      auto executor = make_executor();

      HTTPServer server{
          config,
          executor};

      assert(config.getServerPort() == 18092);
      assert(config.getIOThreads() == 2);
      assert(server.getRouter() != nullptr);
      assert(server.executor() == executor);
      assert(!server.is_stop_requested());

      executor->stop();
    }
  }

  static void test_destructor_is_safe_without_run()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.getRouter() != nullptr);
      assert(!server.is_stop_requested());
    }

    executor->stop();
  }

} // namespace

int main()
{
  test_http_server_type_traits();

  test_constructor_rejects_null_executor();
  test_constructor_accepts_valid_executor();

  test_initial_state_after_construction();
  test_router_is_created_for_each_server();
  test_executor_accessor_returns_original_shared_executor();

  test_server_can_be_constructed_with_different_valid_configs();
  test_destructor_is_safe_without_run();

  return 0;
}
