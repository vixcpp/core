/**
 *
 * @file http_server_shutdown_test.cpp
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
      int port = 18120,
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

  static void test_stop_async_marks_stop_requested()
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

      server.join_threads();
    }

    executor->stop();
  }

  static void test_stop_async_is_idempotent()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.is_stop_requested() == false);

      server.stop_async();
      server.stop_async();
      server.stop_async();

      assert(server.is_stop_requested() == true);

      server.join_threads();
    }

    executor->stop();
  }

  static void test_join_threads_is_safe_before_run()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.is_stop_requested() == false);
      assert(server.bound_port() == 0);

      server.join_threads();
      server.join_threads();

      assert(server.is_stop_requested() == false);
      assert(server.bound_port() == 0);
    }

    executor->stop();
  }

  static void test_stop_blocking_marks_stop_and_joins()
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
    }

    executor->stop();
  }

  static void test_stop_blocking_is_idempotent()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.stop_blocking();
      server.stop_blocking();
      server.stop_blocking();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);
    }

    executor->stop();
  }

  static void test_monitor_metrics_can_start_and_stop_cleanly()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      assert(server.is_stop_requested() == false);

      server.monitor_metrics();

      assert(server.is_stop_requested() == false);

      server.stop_blocking();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);
    }

    executor->stop();
  }

  static void test_monitor_metrics_is_idempotent()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.monitor_metrics();
      server.monitor_metrics();
      server.monitor_metrics();

      assert(server.is_stop_requested() == false);

      server.stop_blocking();

      assert(server.is_stop_requested() == true);
    }

    executor->stop();
  }

  static void test_stop_async_wakes_monitor_thread()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.monitor_metrics();

      assert(server.is_stop_requested() == false);

      server.stop_async();

      assert(server.is_stop_requested() == true);

      server.join_threads();

      assert(server.is_stop_requested() == true);
    }

    executor->stop();
  }

  static void test_join_after_stop_async_is_idempotent()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.stop_async();

      assert(server.is_stop_requested() == true);

      server.join_threads();
      server.join_threads();
      server.join_threads();

      assert(server.is_stop_requested() == true);
      assert(server.bound_port() == 0);
    }

    executor->stop();
  }

  static void test_destructor_stops_server_without_explicit_stop()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.monitor_metrics();

      assert(server.is_stop_requested() == false);
      assert(server.bound_port() == 0);
    }

    executor->stop();
  }

  static void test_destructor_after_stop_blocking_is_safe()
  {
    Config config = make_config();
    auto executor = make_executor();

    {
      HTTPServer server{
          config,
          executor};

      server.monitor_metrics();
      server.stop_blocking();

      assert(server.is_stop_requested() == true);
    }

    executor->stop();
  }

  static void test_multiple_servers_can_shutdown_independently()
  {
    Config first_config = make_config(18121, 1);
    auto first_executor = make_executor();

    Config second_config = make_config(18122, 1);
    auto second_executor = make_executor();

    {
      HTTPServer first{
          first_config,
          first_executor};

      HTTPServer second{
          second_config,
          second_executor};

      assert(first.is_stop_requested() == false);
      assert(second.is_stop_requested() == false);

      first.stop_async();

      assert(first.is_stop_requested() == true);
      assert(second.is_stop_requested() == false);

      second.stop_blocking();

      assert(second.is_stop_requested() == true);

      first.join_threads();
    }

    first_executor->stop();
    second_executor->stop();
  }

} // namespace

int main()
{
  test_stop_async_marks_stop_requested();
  test_stop_async_is_idempotent();

  test_join_threads_is_safe_before_run();

  test_stop_blocking_marks_stop_and_joins();
  test_stop_blocking_is_idempotent();

  test_monitor_metrics_can_start_and_stop_cleanly();
  test_monitor_metrics_is_idempotent();
  test_stop_async_wakes_monitor_thread();

  test_join_after_stop_async_is_idempotent();

  test_destructor_stops_server_without_explicit_stop();
  test_destructor_after_stop_blocking_is_safe();

  test_multiple_servers_can_shutdown_independently();

  return 0;
}
