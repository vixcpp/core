/**
 *
 * @file app_constructor_test.cpp
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
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <vix/app/App.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/server/HTTPServer.hpp>

namespace
{
  using App = vix::App;
  using Config = vix::config::Config;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
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

  static void unset_env_var(const char *name)
  {
#if defined(_WIN32)
    const std::string assignment = std::string{name} + "=";
    const int rc = _putenv(assignment.c_str());
    assert(rc == 0);
#else
    const int rc = unsetenv(name);
    assert(rc == 0);
#endif
  }

  static void prepare_app_env()
  {
    /*
     * Keep App constructor tests deterministic and quiet.
     * The constructor can read these variables when configuring logs,
     * docs and access logging.
     */
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("VIX_DOCS", "false");
    set_env_var("VIX_ACCESS_LOGS", "false");
    set_env_var("VIX_INTERNAL_LOGS", "false");
    set_env_var("VIX_LOG_ASYNC", "false");
    set_env_var("VIX_LOG_LEVEL", "critical");

    unset_env_var("SERVER_PORT");
    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");
  }

  static void test_app_type_traits()
  {
    static_assert(std::is_default_constructible_v<App>);

    static_assert(!std::is_copy_constructible_v<App>);
    static_assert(!std::is_copy_assignable_v<App>);

    static_assert(!std::is_move_constructible_v<App>);
    static_assert(!std::is_move_assignable_v<App>);

    static_assert(std::is_destructible_v<App>);
  }

  static void test_default_constructor_initializes_core_objects()
  {
    prepare_app_env();

    App app;

    assert(app.router() != nullptr);

    /*
     * These accessors return references. Taking their address verifies that
     * the objects are present and usable without starting the server.
     */
    assert(&app.config() != nullptr);
    assert(&app.server() != nullptr);
    assert(&app.executor() != nullptr);

    app.close();
  }

  static void test_default_constructor_starts_executor()
  {
    prepare_app_env();

    App app;

    assert(app.executor().started() == true);
    assert(app.executor().running() == true);
    assert(app.executor().accepting() == true);

    app.close();
  }

  static void test_default_constructor_does_not_start_server()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_default_constructor_uses_default_config()
  {
    prepare_app_env();

    App app;

    assert(app.config().getServerPort() == 8080);
    assert(app.config().getRequestTimeout() == 2000);
    assert(app.config().getSessionTimeoutSec() == 20);

    assert(app.config().isTlsEnabled() == false);
    assert(app.config().getTlsCertFile() == "");
    assert(app.config().getTlsKeyFile() == "");

    app.close();
  }

  static void test_default_constructor_dev_mode_is_disabled()
  {
    prepare_app_env();

    App app;

    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_default_constructor_views_are_not_configured()
  {
    prepare_app_env();

    App app;

    assert(app.has_views() == false);

    bool threw_mutable = false;
    try
    {
      (void)app.views();
    }
    catch (const std::runtime_error &)
    {
      threw_mutable = true;
    }

    assert(threw_mutable == true);

    const App &const_app = app;

    bool threw_const = false;
    try
    {
      (void)const_app.views();
    }
    catch (const std::runtime_error &)
    {
      threw_const = true;
    }

    assert(threw_const == true);

    app.close();
  }

  static void test_default_constructor_registers_bench_route()
  {
    prepare_app_env();

    App app;

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/bench") == true);

    app.close();
  }

  static void test_external_executor_constructor_accepts_valid_executor()
  {
    prepare_app_env();

    auto executor =
        std::make_shared<RuntimeExecutor>(
            RuntimeConfig{
                1u,
                vix::runtime::BudgetConfig{8u}});

    assert(executor->started() == false);
    assert(executor->running() == false);
    assert(executor->accepting() == false);

    {
      App app{executor};

      assert(&app.executor() == executor.get());

      assert(executor->started() == true);
      assert(executor->running() == true);
      assert(executor->accepting() == true);

      assert(app.router() != nullptr);
      assert(app.is_running() == false);
      assert(app.has_server_ready_info() == false);

      app.close();
    }

    /*
     * App does not own this shared_ptr exclusively. The external executor is
     * still valid here and can be stopped by the test.
     */
    executor->stop();

    assert(executor->started() == false);
    assert(executor->running() == false);
    assert(executor->accepting() == false);
  }

  static void test_external_executor_constructor_is_idempotent_when_executor_already_started()
  {
    prepare_app_env();

    auto executor = std::make_shared<RuntimeExecutor>(1u);

    executor->start();

    assert(executor->started() == true);
    assert(executor->running() == true);
    assert(executor->accepting() == true);

    {
      App app{executor};

      assert(&app.executor() == executor.get());

      assert(executor->started() == true);
      assert(executor->running() == true);
      assert(executor->accepting() == true);

      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);

      app.close();
    }

    executor->stop();

    assert(executor->started() == false);
    assert(executor->running() == false);
    assert(executor->accepting() == false);
  }

  static void test_external_executor_constructor_rejects_null_executor()
  {
    prepare_app_env();

    bool threw = false;

    try
    {
      std::shared_ptr<RuntimeExecutor> executor{};
      App app{executor};
      (void)app;
    }
    catch (const std::exception &)
    {
      threw = true;
    }

    assert(threw == true);
  }

  static void test_close_before_listen_is_safe_and_idempotent()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);

    app.close();
    app.close();
    app.close();

    assert(app.is_running() == false);
  }

  static void test_request_stop_from_signal_before_listen_is_safe()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);

    app.request_stop_from_signal();

    assert(app.is_running() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_config_accessor_is_mutable()
  {
    prepare_app_env();

    App app;

    assert(app.config().getServerPort() == 8080);

    app.config().setServerPort(18080);

    assert(app.config().getServerPort() == 18080);
    assert(app.config().has("server.port") == true);
    assert(app.config().getInt("server.port", -1) == 18080);

    app.close();
  }

  static void test_router_accessor_returns_stable_shared_router()
  {
    prepare_app_env();

    App app;

    auto first = app.router();
    auto second = app.router();

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first.get() == second.get());

    app.close();
  }

  static void test_server_accessor_returns_stable_reference()
  {
    prepare_app_env();

    App app;

    auto *first = &app.server();
    auto *second = &app.server();

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first == second);

    app.close();
  }

  static void test_executor_accessor_returns_stable_reference()
  {
    prepare_app_env();

    App app;

    auto *first = &app.executor();
    auto *second = &app.executor();

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first == second);

    app.close();
  }

} // namespace

int main()
{
  test_app_type_traits();

  test_default_constructor_initializes_core_objects();
  test_default_constructor_starts_executor();
  test_default_constructor_does_not_start_server();
  test_default_constructor_uses_default_config();

  test_default_constructor_dev_mode_is_disabled();

  test_default_constructor_views_are_not_configured();

  test_default_constructor_registers_bench_route();

  test_external_executor_constructor_accepts_valid_executor();
  test_external_executor_constructor_is_idempotent_when_executor_already_started();
  test_external_executor_constructor_rejects_null_executor();

  test_close_before_listen_is_safe_and_idempotent();
  test_request_stop_from_signal_before_listen_is_safe();

  test_config_accessor_is_mutable();

  test_router_accessor_returns_stable_shared_router();
  test_server_accessor_returns_stable_reference();
  test_executor_accessor_returns_stable_reference();

  return 0;
}
