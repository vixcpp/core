/**
 *
 * @file http_server_run_validation_test.cpp
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

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
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

  static Config make_config(
      int port = 18130,
      int io_threads = 1)
  {
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("SERVER_PORT", std::to_string(port));
    set_env_var("SERVER_IO_THREADS", std::to_string(io_threads));
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("LOGGING_ASYNC", "false");

    Config config{};

    assert(config.getServerPort() == port);
    assert(config.getIOThreads() == io_threads);
    assert(config.isTlsEnabled() == false);

    return config;
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
  }

  static void assert_run_after_stop_throws(HTTPServer &server)
  {
    bool threw = false;

    try
    {
      server.run();
    }
    catch (const std::runtime_error &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("cannot run server after stop was requested") != std::string::npos);
    }

    assert(threw);
  }

  static void test_run_after_stop_async_throws()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.is_stop_requested() == false);
      assert(server.bound_port() == 0);

      server.stop_async();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      assert_run_after_stop_throws(server);

      server.join_threads();
    }

    executor->stop();
  }

  static void test_run_after_stop_blocking_throws()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.is_stop_requested() == false);

      server.stop_blocking();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      assert_run_after_stop_throws(server);
    }

    executor->stop();
  }

  static void test_run_after_repeated_stop_async_throws()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.stop_async();
      server.stop_async();
      server.stop_async();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      assert_run_after_stop_throws(server);

      server.join_threads();
    }

    executor->stop();
  }

  static void test_run_after_monitor_then_stop_throws()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.monitor_metrics();

      assert(server.is_stop_requested() == false);

      server.stop_blocking();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      assert_run_after_stop_throws(server);
    }

    executor->stop();
  }

  static void test_start_accept_after_stop_async_is_noop()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.stop_async();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      server.start_accept();
      server.start_accept();
      server.start_accept();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      server.join_threads();
    }

    executor->stop();
  }

  static void test_start_accept_after_stop_blocking_is_noop()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.stop_blocking();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      server.start_accept();
      server.start_accept();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);
    }

    executor->stop();
  }

  static void test_stop_then_run_does_not_create_bound_port()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.bound_port() == 0);

      server.stop_async();

      assert(server.bound_port() == 0);

      assert_run_after_stop_throws(server);

      assert(server.bound_port() == 0);

      server.join_threads();
    }

    executor->stop();
  }

  static void test_router_and_executor_remain_accessible_after_stop()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      auto router = server.getRouter();

      assert(router != nullptr);
      assert(server.executor() == executor);

      server.stop_blocking();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);

      assert(server.getRouter() == router);
      assert(server.executor() == executor);

      assert_run_after_stop_throws(server);
    }

    executor->stop();
  }

  static void test_multiple_servers_reject_run_independently_after_stop()
  {
    Config first_config = make_config(18131, 1);
    auto first_executor = make_executor();

    Config second_config = make_config(18132, 1);
    auto second_executor = make_executor();

    {
      HTTPServer first{
          first_config,
          first_executor};

      HTTPServer second{
          second_config,
          second_executor};

      first.stop_async();

      assert(first.is_stop_requested() == true);
      assert(second.is_stop_requested() == false);

      assert_run_after_stop_throws(first);

      second.stop_blocking();

      assert(second.is_stop_requested() == true);

      assert_run_after_stop_throws(second);

      first.join_threads();
    }

    first_executor->stop();
    second_executor->stop();
  }

} // namespace

int main()
{
  test_run_after_stop_async_throws();
  test_run_after_stop_blocking_throws();
  test_run_after_repeated_stop_async_throws();
  test_run_after_monitor_then_stop_throws();

  test_start_accept_after_stop_async_is_noop();
  test_start_accept_after_stop_blocking_is_noop();

  test_stop_then_run_does_not_create_bound_port();
  test_router_and_executor_remain_accessible_after_stop();

  test_multiple_servers_reject_run_independently_after_stop();

  return 0;
}
