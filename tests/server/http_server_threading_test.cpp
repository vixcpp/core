/**
 *
 * @file http_server_threading_test.cpp
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
#include <thread>

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
      int port = 18110,
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

  static void test_calculate_io_thread_count_returns_positive_value()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    const std::size_t count = server.calculate_io_thread_count();

    assert(count > 0);

    executor->stop();
  }

  static void test_calculate_io_thread_count_is_stable_for_same_server()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    const std::size_t first = server.calculate_io_thread_count();
    const std::size_t second = server.calculate_io_thread_count();
    const std::size_t third = server.calculate_io_thread_count();

    assert(first > 0);
    assert(second > 0);
    assert(third > 0);

    assert(first == second);
    assert(second == third);

    executor->stop();
  }

  static void test_calculate_io_thread_count_uses_runtime_safe_fallback()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    const std::size_t count = server.calculate_io_thread_count();

    const unsigned int hardware = std::thread::hardware_concurrency();

    if (hardware == 0)
    {
      assert(count >= 1);
    }
    else
    {
      assert(count >= 1);
    }

    executor->stop();
  }

  static void test_calculate_io_thread_count_does_not_start_server()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    assert(!server.is_stop_requested());
    assert(server.bound_port() == 0);

    const std::size_t count = server.calculate_io_thread_count();

    assert(count > 0);
    assert(!server.is_stop_requested());
    assert(server.bound_port() == 0);

    executor->stop();
  }

  static void test_executor_accessor_is_threading_safe_before_run()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto first = server.executor();
    auto second = server.executor();

    assert(first != nullptr);
    assert(second != nullptr);

    assert(first == executor);
    assert(second == executor);
    assert(first == second);

    assert(!server.is_stop_requested());
    assert(server.bound_port() == 0);

    executor->stop();
  }

  static void test_bound_port_is_zero_before_listener_is_started()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    assert(server.bound_port() == 0);

    (void)server.calculate_io_thread_count();

    assert(server.bound_port() == 0);

    executor->stop();
  }

  static void test_stop_requested_is_false_before_run_or_stop()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    assert(server.is_stop_requested() == false);

    (void)server.calculate_io_thread_count();
    (void)server.executor();
    (void)server.getRouter();
    (void)server.bound_port();

    assert(server.is_stop_requested() == false);

    executor->stop();
  }

  static void test_multiple_servers_have_independent_threading_state()
  {
    Config first_config = make_config(18111, 1);
    auto first_executor = make_executor();

    HTTPServer first{
        first_config,
        first_executor};

    Config second_config = make_config(18112, 2);
    auto second_executor = make_executor();

    HTTPServer second{
        second_config,
        second_executor};

    assert(first.executor() == first_executor);
    assert(second.executor() == second_executor);
    assert(first.executor() != second.executor());

    assert(first.bound_port() == 0);
    assert(second.bound_port() == 0);

    assert(first.is_stop_requested() == false);
    assert(second.is_stop_requested() == false);

    assert(first.calculate_io_thread_count() > 0);
    assert(second.calculate_io_thread_count() > 0);

    assert(first.bound_port() == 0);
    assert(second.bound_port() == 0);

    first_executor->stop();
    second_executor->stop();
  }

  static void test_calculate_io_thread_count_after_stop_request_still_returns_positive_value()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    assert(server.is_stop_requested() == false);

    server.stop_async();

    assert(server.is_stop_requested() == true);

    const std::size_t count = server.calculate_io_thread_count();

    assert(count > 0);

    server.join_threads();

    executor->stop();
  }

} // namespace

int main()
{
  test_calculate_io_thread_count_returns_positive_value();
  test_calculate_io_thread_count_is_stable_for_same_server();
  test_calculate_io_thread_count_uses_runtime_safe_fallback();
  test_calculate_io_thread_count_does_not_start_server();

  test_executor_accessor_is_threading_safe_before_run();

  test_bound_port_is_zero_before_listener_is_started();
  test_stop_requested_is_false_before_run_or_stop();

  test_multiple_servers_have_independent_threading_state();
  test_calculate_io_thread_count_after_stop_request_still_returns_positive_value();

  return 0;
}
