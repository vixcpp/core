/**
 *
 * @file app_templates_test.cpp
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
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include <vix/app/App.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/template/Context.hpp>
#include <vix/view/TemplateView.hpp>

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

    unset_env_var("SERVER_PORT");
    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");
  }

  static std::filesystem::path make_temp_dir(const std::string &name)
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        (name + "_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    assert(!ec);

    return dir;
  }

  static void write_file(const std::filesystem::path &path, const std::string &content)
  {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    assert(!ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);

    out << content;
    out.close();

    assert(std::filesystem::exists(path));
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(
        RuntimeConfig{
            1u,
            vix::runtime::BudgetConfig{8u}});
  }

  static void test_views_are_disabled_by_default()
  {
    prepare_app_env();

    App app;

    assert(app.has_views() == false);

    app.close();
  }

  static void test_mutable_views_throws_before_templates()
  {
    prepare_app_env();

    App app;

    bool threw = false;

    try
    {
      (void)app.views();
    }
    catch (const std::runtime_error &)
    {
      threw = true;
    }

    assert(threw == true);
    assert(app.has_views() == false);

    app.close();
  }

  static void test_const_views_throws_before_templates()
  {
    prepare_app_env();

    App app;

    const App &const_app = app;

    bool threw = false;

    try
    {
      (void)const_app.views();
    }
    catch (const std::runtime_error &)
    {
      threw = true;
    }

    assert(threw == true);
    assert(const_app.has_views() == false);

    app.close();
  }

  static void test_templates_returns_app_reference()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_return_ref");

    App app;

    App &returned = app.templates(views_dir.string());

    assert(&returned == &app);
    assert(app.has_views() == true);

    app.close();
  }

  static void test_templates_enables_views_for_existing_empty_directory()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_empty");

    App app;

    assert(app.has_views() == false);

    app.templates(views_dir.string());

    assert(app.has_views() == true);

    (void)app.views();

    const App &const_app = app;
    (void)const_app.views();

    app.close();
  }

  static void test_templates_enables_views_for_directory_with_template_files()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_files");

    write_file(
        views_dir / "index.html",
        "<!doctype html><html><body>{{ title }}</body></html>");

    write_file(
        views_dir / "users" / "show.html",
        "<!doctype html><html><body>User {{ id }}</body></html>");

    App app;

    app.templates(views_dir.string());

    assert(app.has_views() == true);

    (void)app.views();

    app.close();
  }

  static void test_views_reference_is_stable_between_calls()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_stable_ref");

    App app;

    app.templates(views_dir.string());

    auto *first = &app.views();
    auto *second = &app.views();

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first == second);

    app.close();
  }

  static void test_const_views_reference_matches_mutable_views_reference()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_const_ref");

    App app;

    app.templates(views_dir.string());

    auto *mutable_view = &app.views();

    const App &const_app = app;
    auto *const_view = &const_app.views();

    assert(mutable_view != nullptr);
    assert(const_view != nullptr);
    assert(mutable_view == const_view);

    app.close();
  }

  static void test_templates_can_be_called_more_than_once()
  {
    prepare_app_env();

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_templates_reconfigure_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_templates_reconfigure_second");

    write_file(first_dir / "index.html", "first");
    write_file(second_dir / "index.html", "second");

    App app;

    app.templates(first_dir.string());

    assert(app.has_views() == true);
    (void)app.views();

    app.templates(second_dir.string());

    assert(app.has_views() == true);
    (void)app.views();

    app.close();
  }

  static void test_templates_with_relative_directory_string()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_templates_relative_root");

    const auto old_cwd = std::filesystem::current_path();

    std::filesystem::current_path(root);

    std::error_code ec;
    std::filesystem::create_directories("views", ec);
    assert(!ec);

    write_file("views/index.html", "relative");

    App app;

    app.templates("views");

    assert(app.has_views() == true);
    (void)app.views();

    app.close();

    std::filesystem::current_path(old_cwd);
  }

  static void test_templates_with_nested_directory()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_templates_nested");

    const std::filesystem::path views_dir = root / "resources" / "views";

    write_file(views_dir / "index.html", "nested");

    App app;

    app.templates(views_dir.string());

    assert(app.has_views() == true);
    (void)app.views();

    app.close();
  }

  static void test_templates_with_missing_directory_still_configures_view_facade()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_templates_missing_root");

    const std::filesystem::path missing_dir = root / "missing_views";

    assert(std::filesystem::exists(missing_dir) == false);

    App app;

    app.templates(missing_dir.string());

    assert(app.has_views() == true);
    (void)app.views();

    app.close();
  }

  static void test_templates_do_not_start_server()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_no_server");

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.templates(views_dir.string());

    assert(app.has_views() == true);
    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_templates_do_not_change_dev_mode()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_dev_mode");

    App app;

    assert(app.isDevMode() == false);

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.templates(views_dir.string());

    assert(app.has_views() == true);
    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert(app.has_views() == true);

    app.close();
  }

  static void test_templates_do_not_change_config_values()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_config");

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    app.templates(views_dir.string());

    assert(app.has_views() == true);
    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    app.close();
  }

  static void test_templates_before_route_registration_keeps_route_registration_valid()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_before_route");

    write_file(views_dir / "index.html", "<html>home</html>");

    App app;

    app.templates(views_dir.string());

    app.get(
        "/",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("home");
        });

    assert(app.has_views() == true);
    assert(app.router()->has_route("GET", "/") == true);
    assert(app.router()->has_route("OPTIONS", "/") == true);

    app.close();
  }

  static void test_templates_after_route_registration_keeps_existing_route_registered()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_after_route");

    App app;

    app.get(
        "/before",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("before");
        });

    assert(app.router()->has_route("GET", "/before") == true);
    assert(app.router()->has_route("OPTIONS", "/before") == true);
    assert(app.has_views() == false);

    app.templates(views_dir.string());

    assert(app.has_views() == true);
    assert(app.router()->has_route("GET", "/before") == true);
    assert(app.router()->has_route("OPTIONS", "/before") == true);

    app.close();
  }

  static void test_templates_are_available_to_routes_registered_after_reconfiguration()
  {
    prepare_app_env();

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_templates_route_reconfig_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_templates_route_reconfig_second");

    write_file(first_dir / "index.html", "first");
    write_file(second_dir / "index.html", "second");

    App app;

    app.templates(first_dir.string());

    app.get(
        "/first",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("first");
        });

    app.templates(second_dir.string());

    app.get(
        "/second",
        [](vix::http::Request &, vix::http::ResponseWrapper &res)
        {
          res.ok().text("second");
        });

    assert(app.has_views() == true);

    assert(app.router()->has_route("GET", "/first") == true);
    assert(app.router()->has_route("OPTIONS", "/first") == true);

    assert(app.router()->has_route("GET", "/second") == true);
    assert(app.router()->has_route("OPTIONS", "/second") == true);

    app.close();
  }

  static void test_templates_with_external_executor_constructor()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_external_executor");

    auto executor = make_executor();

    App app{executor};

    assert(app.has_views() == false);
    assert(executor->started() == true);
    assert(executor->running() == true);

    app.templates(views_dir.string());

    assert(app.has_views() == true);
    (void)app.views();

    app.close();
    executor->stop();
  }

  static void test_templates_reconfiguration_with_external_executor_constructor()
  {
    prepare_app_env();

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_templates_external_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_templates_external_second");

    auto executor = make_executor();

    App app{executor};

    app.templates(first_dir.string());
    assert(app.has_views() == true);

    app.templates(second_dir.string());
    assert(app.has_views() == true);

    (void)app.views();

    app.close();
    executor->stop();
  }

  static void test_templates_survive_request_stop_from_signal_before_listen()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_signal");

    App app;

    app.templates(views_dir.string());

    assert(app.has_views() == true);

    app.request_stop_from_signal();

    assert(app.has_views() == true);
    assert(app.is_running() == false);

    (void)app.views();

    app.close();
  }

  static void test_close_does_not_clear_views()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_close");

    App app;

    app.templates(views_dir.string());

    assert(app.has_views() == true);

    app.close();

    assert(app.is_running() == false);
    assert(app.has_views() == true);

    (void)app.views();

    app.close();
  }

  static void test_multiple_apps_have_independent_template_state()
  {
    prepare_app_env();

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_templates_multi_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_templates_multi_second");

    App first;
    App second;

    assert(first.has_views() == false);
    assert(second.has_views() == false);

    first.templates(first_dir.string());

    assert(first.has_views() == true);
    assert(second.has_views() == false);

    second.templates(second_dir.string());

    assert(first.has_views() == true);
    assert(second.has_views() == true);

    assert(&first.views() != &second.views());

    first.close();
    second.close();
  }

  static void test_template_context_type_is_usable_with_app_templates()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_templates_context");

    write_file(
        views_dir / "index.html",
        "<html><title>{{ title }}</title><body>{{ message }}</body></html>");

    App app;

    app.templates(views_dir.string());

    vix::template_::Context ctx;
    ctx.set("title", "Home");
    ctx.set("message", "Hello from Vix");

    assert(app.has_views() == true);
    (void)app.views();
    (void)ctx;

    app.close();
  }

} // namespace

int main()
{
  test_views_are_disabled_by_default();

  test_mutable_views_throws_before_templates();
  test_const_views_throws_before_templates();

  test_templates_returns_app_reference();
  test_templates_enables_views_for_existing_empty_directory();
  test_templates_enables_views_for_directory_with_template_files();

  test_views_reference_is_stable_between_calls();
  test_const_views_reference_matches_mutable_views_reference();

  test_templates_can_be_called_more_than_once();

  test_templates_with_relative_directory_string();
  test_templates_with_nested_directory();
  test_templates_with_missing_directory_still_configures_view_facade();

  test_templates_do_not_start_server();
  test_templates_do_not_change_dev_mode();
  test_templates_do_not_change_config_values();

  test_templates_before_route_registration_keeps_route_registration_valid();
  test_templates_after_route_registration_keeps_existing_route_registered();
  test_templates_are_available_to_routes_registered_after_reconfiguration();

  test_templates_with_external_executor_constructor();
  test_templates_reconfiguration_with_external_executor_constructor();

  test_templates_survive_request_stop_from_signal_before_listen();
  test_close_does_not_clear_views();

  test_multiple_apps_have_independent_template_state();

  test_template_context_type_is_usable_with_app_templates();

  return 0;
}
