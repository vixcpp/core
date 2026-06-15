/**
 *
 * @file app_module_init_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <vix/app/App.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>

namespace
{
  using App = vix::App;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;

  static std::atomic<int> g_first_module_calls{0};
  static std::atomic<int> g_second_module_calls{0};
  static std::atomic<int> g_replaced_module_calls{0};
  static std::atomic<int> g_post_once_module_calls{0};
  static std::atomic<int> g_throwing_module_calls{0};
  static std::atomic<int> g_external_executor_module_calls{0};
  static std::atomic<int> g_route_module_calls{0};
  static std::atomic<int> g_config_module_calls{0};
  static std::atomic<int> g_dev_mode_module_calls{0};

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

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(
        RuntimeConfig{
            1u,
            vix::runtime::BudgetConfig{8u}});
  }

  static void reset_counters()
  {
    g_first_module_calls.store(0, std::memory_order_relaxed);
    g_second_module_calls.store(0, std::memory_order_relaxed);
    g_replaced_module_calls.store(0, std::memory_order_relaxed);
    g_post_once_module_calls.store(0, std::memory_order_relaxed);
    g_throwing_module_calls.store(0, std::memory_order_relaxed);
    g_external_executor_module_calls.store(0, std::memory_order_relaxed);
    g_route_module_calls.store(0, std::memory_order_relaxed);
    g_config_module_calls.store(0, std::memory_order_relaxed);
    g_dev_mode_module_calls.store(0, std::memory_order_relaxed);
  }

  static void test_module_init_fn_type_traits()
  {
    static_assert(std::is_copy_constructible_v<App::ModuleInitFn>);
    static_assert(std::is_move_constructible_v<App::ModuleInitFn>);
    static_assert(std::is_copy_assignable_v<App::ModuleInitFn>);
    static_assert(std::is_move_assignable_v<App::ModuleInitFn>);
    static_assert(std::is_destructible_v<App::ModuleInitFn>);
  }

  static void test_set_module_init_accepts_empty_function_before_first_app()
  {
    prepare_app_env();

    App::set_module_init({});
    App::set_module_init(nullptr);

    assert(g_first_module_calls.load(std::memory_order_relaxed) == 0);
    assert(g_second_module_calls.load(std::memory_order_relaxed) == 0);
  }

  static void test_set_module_init_replaces_previous_function_before_first_app()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_first_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    App::set_module_init(
        []
        {
          g_second_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(g_first_module_calls.load(std::memory_order_relaxed) == 0);
    assert(g_second_module_calls.load(std::memory_order_relaxed) == 0);

    App app;

    assert(g_first_module_calls.load(std::memory_order_relaxed) == 0);
    assert(g_second_module_calls.load(std::memory_order_relaxed) == 1);

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/bench") == true);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_module_init_runs_only_once_for_multiple_default_apps()
  {
    prepare_app_env();

    const int before_first =
        g_first_module_calls.load(std::memory_order_relaxed);

    const int before_second =
        g_second_module_calls.load(std::memory_order_relaxed);

    {
      App first;
      assert(first.router() != nullptr);
      assert(first.router()->has_route("GET", "/bench") == true);
      first.close();
    }

    {
      App second;
      assert(second.router() != nullptr);
      assert(second.router()->has_route("GET", "/bench") == true);
      second.close();
    }

    {
      App third;
      assert(third.router() != nullptr);
      assert(third.router()->has_route("GET", "/bench") == true);
      third.close();
    }

    assert(g_first_module_calls.load(std::memory_order_relaxed) == before_first);
    assert(g_second_module_calls.load(std::memory_order_relaxed) == before_second);
  }

  static void test_setting_new_module_init_after_once_does_not_run_for_new_apps()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_post_once_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    {
      App app;
      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);
      app.close();
    }

    {
      App app;
      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);
      app.close();
    }

    assert(g_post_once_module_calls.load(std::memory_order_relaxed) == 0);
  }

  static void test_replacing_module_init_after_once_does_not_run_replacement()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_replaced_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    App::set_module_init(
        []
        {
          g_post_once_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    App app;

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/bench") == true);

    assert(g_replaced_module_calls.load(std::memory_order_relaxed) == 0);
    assert(g_post_once_module_calls.load(std::memory_order_relaxed) == 0);

    app.close();
  }

  static void test_throwing_module_init_set_after_once_is_not_executed()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_throwing_module_calls.fetch_add(1, std::memory_order_relaxed);
          throw std::runtime_error("module init should not run after once");
        });

    bool threw = false;

    try
    {
      App app;
      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);
      app.close();
    }
    catch (...)
    {
      threw = true;
    }

    assert(threw == false);
    assert(g_throwing_module_calls.load(std::memory_order_relaxed) == 0);
  }

  static void test_empty_module_init_set_after_once_is_safe()
  {
    prepare_app_env();

    App::set_module_init({});
    App::set_module_init(nullptr);

    App app;

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/bench") == true);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_external_executor_constructor_does_not_rerun_module_init_after_once()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_external_executor_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    auto executor = make_executor();

    {
      App app{executor};

      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);
      assert(&app.executor() == executor.get());
      assert(app.is_running() == false);

      app.close();
    }

    {
      App app{executor};

      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);
      assert(&app.executor() == executor.get());
      assert(app.is_running() == false);

      app.close();
    }

    assert(g_external_executor_module_calls.load(std::memory_order_relaxed) == 0);

    executor->stop();
  }

  static void test_module_init_once_does_not_block_route_registration()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_route_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    App app;

    app.get(
        "/module/routes",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("ok");
        });

    app.post(
        "/module/routes",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("created");
        });

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/module/routes") == true);
    assert(app.router()->has_route("POST", "/module/routes") == true);
    assert(app.router()->has_route("OPTIONS", "/module/routes") == true);

    assert(g_route_module_calls.load(std::memory_order_relaxed) == 0);

    app.close();
  }

  static void test_module_init_once_does_not_block_group_registration()
  {
    prepare_app_env();

    App app;

    app.group(
        "/module",
        [](App::Group group)
        {
          group.get(
              "/group",
              [](vix::http::Request &, vix::http::ResponseWrapper &res)
              {
                res.ok().text("group");
              });

          group.post(
              "/group",
              [](vix::http::Request &, vix::http::ResponseWrapper &res)
              {
                res.ok().text("group post");
              });
        });

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/module/group") == true);
    assert(app.router()->has_route("POST", "/module/group") == true);
    assert(app.router()->has_route("OPTIONS", "/module/group") == true);

    app.close();
  }

  static void test_module_init_once_does_not_block_middleware_registration()
  {
    prepare_app_env();

    App app;

    bool middleware_executed = false;

    app.use(
        [&middleware_executed](
            vix::http::Request &,
            vix::http::ResponseWrapper &,
            App::Next next)
        {
          middleware_executed = true;
          next();
        });

    app.protect(
        "/module/private",
        [](
            vix::http::Request &,
            vix::http::ResponseWrapper &,
            App::Next next)
        {
          next();
        });

    app.get(
        "/module/private/status",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("private");
        });

    /*
     * Registration alone must not execute middleware.
     */
    assert(middleware_executed == false);

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/module/private/status") == true);
    assert(app.router()->has_route("OPTIONS", "/module/private/status") == true);

    app.close();
  }

  static void test_module_init_once_does_not_block_config_mutation()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_config_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    App app;

    app.config().setServerPort(18080);
    app.config().set("module.config.enabled", true);
    app.config().set("module.config.name", "module-init");

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getBool("module.config.enabled", false) == true);
    assert(app.config().getString("module.config.name", "missing") == "module-init");

    assert(g_config_module_calls.load(std::memory_order_relaxed) == 0);

    app.close();
  }

  static void test_module_init_once_does_not_block_dev_mode_toggle()
  {
    prepare_app_env();

    App::set_module_init(
        []
        {
          g_dev_mode_module_calls.fetch_add(1, std::memory_order_relaxed);
        });

    App app;

    assert(app.isDevMode() == false);

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    assert(g_dev_mode_module_calls.load(std::memory_order_relaxed) == 0);

    app.close();
  }

  static void test_module_init_once_does_not_block_templates_configuration()
  {
    prepare_app_env();

    App app;

    assert(app.has_views() == false);

    app.templates("views");

    assert(app.has_views() == true);

    app.close();
  }

  static void test_module_init_once_does_not_block_close_before_listen()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);

    app.close();
    app.close();

    assert(app.is_running() == false);
  }

  static void test_module_init_once_does_not_block_request_stop_before_listen()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);

    app.request_stop_from_signal();

    assert(app.is_running() == false);

    app.close();
  }

  static void test_module_init_once_keeps_ready_info_empty_before_listen()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();
  }

  static void test_module_init_once_with_many_app_instances()
  {
    prepare_app_env();

    const int first_before =
        g_first_module_calls.load(std::memory_order_relaxed);

    const int second_before =
        g_second_module_calls.load(std::memory_order_relaxed);

    for (int i = 0; i < 20; ++i)
    {
      App app;

      assert(app.router() != nullptr);
      assert(app.router()->has_route("GET", "/bench") == true);
      assert(app.is_running() == false);

      app.get(
          "/many/" + std::to_string(i),
          [](vix::http::Request &, vix::http::ResponseWrapper &res)
          {
            res.ok().text("many");
          });

      assert(app.router()->has_route("GET", "/many/" + std::to_string(i)) == true);
      assert(app.router()->has_route("OPTIONS", "/many/" + std::to_string(i)) == true);

      app.close();
    }

    assert(g_first_module_calls.load(std::memory_order_relaxed) == first_before);
    assert(g_second_module_calls.load(std::memory_order_relaxed) == second_before);
  }

  static void test_module_init_once_can_be_cleared_at_end()
  {
    prepare_app_env();

    App::set_module_init({});

    App app;

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/bench") == true);

    app.close();
  }

} // namespace

int main()
{
  prepare_app_env();
  reset_counters();

  test_module_init_fn_type_traits();

  /*
   * These two tests must run before any App instance is constructed because
   * App module initialization is intentionally once-per-process.
   */
  test_set_module_init_accepts_empty_function_before_first_app();
  test_set_module_init_replaces_previous_function_before_first_app();

  test_module_init_runs_only_once_for_multiple_default_apps();

  test_setting_new_module_init_after_once_does_not_run_for_new_apps();
  test_replacing_module_init_after_once_does_not_run_replacement();
  test_throwing_module_init_set_after_once_is_not_executed();
  test_empty_module_init_set_after_once_is_safe();

  test_external_executor_constructor_does_not_rerun_module_init_after_once();

  test_module_init_once_does_not_block_route_registration();
  test_module_init_once_does_not_block_group_registration();
  test_module_init_once_does_not_block_middleware_registration();

  test_module_init_once_does_not_block_config_mutation();
  test_module_init_once_does_not_block_dev_mode_toggle();
  test_module_init_once_does_not_block_templates_configuration();

  test_module_init_once_does_not_block_close_before_listen();
  test_module_init_once_does_not_block_request_stop_before_listen();
  test_module_init_once_keeps_ready_info_empty_before_listen();

  test_module_init_once_with_many_app_instances();

  test_module_init_once_can_be_cleared_at_end();

  App::set_module_init({});

  return 0;
}
