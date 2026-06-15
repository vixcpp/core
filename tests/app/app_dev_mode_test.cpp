/**
 *
 * @file app_dev_mode_test.cpp
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

#include <vix/app/App.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>

namespace
{
  using App = vix::App;
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
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("VIX_DOCS", "false");
    set_env_var("VIX_ACCESS_LOGS", "false");
    set_env_var("VIX_INTERNAL_LOGS", "false");
    set_env_var("VIX_LOG_ASYNC", "false");
    set_env_var("VIX_LOG_LEVEL", "critical");

    unset_env_var("VIX_MODE");
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

  static void test_dev_mode_default_is_false()
  {
    prepare_app_env();

    App app;

    assert(app.isDevMode() == false);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_set_dev_mode_true()
  {
    prepare_app_env();

    App app;

    assert(app.isDevMode() == false);

    app.setDevMode(true);

    assert(app.isDevMode() == true);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_set_dev_mode_false()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_dev_mode_toggle_multiple_times()
  {
    prepare_app_env();

    App app;

    assert(app.isDevMode() == false);

    app.setDevMode(true);
    assert(app.isDevMode() == true);

    app.setDevMode(false);
    assert(app.isDevMode() == false);

    app.setDevMode(true);
    assert(app.isDevMode() == true);

    app.setDevMode(true);
    assert(app.isDevMode() == true);

    app.setDevMode(false);
    assert(app.isDevMode() == false);

    app.setDevMode(false);
    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_dev_mode_is_independent_from_config_mutation()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");
    app.config().set("app.debug", true);

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");
    assert(app.config().getBool("app.debug", false) == true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");
    assert(app.config().getBool("app.debug", false) == true);

    app.close();
  }

  static void test_dev_mode_is_independent_from_route_registration()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.get(
        "/dev-mode",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("dev");
        });

    assert(app.router()->has_route("GET", "/dev-mode") == true);
    assert(app.router()->has_route("OPTIONS", "/dev-mode") == true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.router()->has_route("GET", "/dev-mode") == true);
    assert(app.router()->has_route("OPTIONS", "/dev-mode") == true);

    app.close();
  }

  static void test_dev_mode_is_independent_from_heavy_route_registration()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    app.get_heavy(
        "/heavy-dev",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("heavy");
        });

    assert(app.router()->has_route("GET", "/heavy-dev") == true);
    assert(app.router()->has_route("OPTIONS", "/heavy-dev") == true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.router()->has_route("GET", "/heavy-dev") == true);

    app.close();
  }

  static void test_dev_mode_is_independent_from_middleware_registration()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    bool middleware_registered = false;

    app.use(
        [&middleware_registered](
            vix::http::Request &,
            vix::http::ResponseWrapper &,
            App::Next next)
        {
          middleware_registered = true;
          next();
        });

    /*
     * Registration alone does not execute middleware.
     */
    assert(middleware_registered == false);
    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(middleware_registered == false);

    app.close();
  }

  static void test_dev_mode_is_independent_from_group_registration()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get(
              "/status",
              [](vix::http::Request &, vix::http::ResponseWrapper &res)
              {
                res.ok().text("ok");
              });
        });

    assert(app.router()->has_route("GET", "/api/status") == true);
    assert(app.router()->has_route("OPTIONS", "/api/status") == true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.router()->has_route("GET", "/api/status") == true);

    app.close();
  }

  static void test_dev_mode_is_independent_from_static_dir_registration()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    app.static_dir("public", "/assets");

    assert(app.isDevMode() == true);
    assert(app.is_running() == false);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_dev_mode_is_independent_from_templates_configuration()
  {
    prepare_app_env();

    App app;

    assert(app.has_views() == false);

    app.setDevMode(true);

    app.templates("views");

    assert(app.has_views() == true);
    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.has_views() == true);
    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_dev_mode_is_independent_from_request_stop_signal()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);
    assert(app.is_running() == false);

    app.request_stop_from_signal();

    assert(app.isDevMode() == true);
    assert(app.is_running() == false);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_close_does_not_change_dev_mode()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.close();

    assert(app.is_running() == false);
    assert(app.isDevMode() == true);

    app.close();

    assert(app.isDevMode() == true);
  }

  static void test_external_executor_constructor_default_dev_mode_is_false()
  {
    prepare_app_env();

    auto executor = make_executor();

    App app{executor};

    assert(app.isDevMode() == false);
    assert(app.is_running() == false);

    app.close();
    executor->stop();
  }

  static void test_external_executor_constructor_dev_mode_toggle()
  {
    prepare_app_env();

    auto executor = make_executor();

    App app{executor};

    assert(app.isDevMode() == false);

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    app.close();
    executor->stop();
  }

  static void test_multiple_apps_have_independent_dev_mode()
  {
    prepare_app_env();

    App first;
    App second;

    assert(first.isDevMode() == false);
    assert(second.isDevMode() == false);

    first.setDevMode(true);

    assert(first.isDevMode() == true);
    assert(second.isDevMode() == false);

    second.setDevMode(true);

    assert(first.isDevMode() == true);
    assert(second.isDevMode() == true);

    first.setDevMode(false);

    assert(first.isDevMode() == false);
    assert(second.isDevMode() == true);

    first.close();
    second.close();
  }

  static void test_dev_mode_survives_config_assignment()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    vix::config::Config cfg;
    cfg.setServerPort(18090);
    cfg.set("app.name", "assigned");

    app.config() = cfg;

    assert(app.config().getServerPort() == 18090);
    assert(app.config().getString("app.name", "missing") == "assigned");

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.config().getServerPort() == 18090);
    assert(app.config().getString("app.name", "missing") == "assigned");

    app.close();
  }

} // namespace

int main()
{
  test_dev_mode_default_is_false();

  test_set_dev_mode_true();
  test_set_dev_mode_false();
  test_dev_mode_toggle_multiple_times();

  test_dev_mode_is_independent_from_config_mutation();

  test_dev_mode_is_independent_from_route_registration();
  test_dev_mode_is_independent_from_heavy_route_registration();

  test_dev_mode_is_independent_from_middleware_registration();
  test_dev_mode_is_independent_from_group_registration();

  test_dev_mode_is_independent_from_static_dir_registration();
  test_dev_mode_is_independent_from_templates_configuration();

  test_dev_mode_is_independent_from_request_stop_signal();

  test_close_does_not_change_dev_mode();

  test_external_executor_constructor_default_dev_mode_is_false();
  test_external_executor_constructor_dev_mode_toggle();

  test_multiple_apps_have_independent_dev_mode();

  test_dev_mode_survives_config_assignment();

  return 0;
}
