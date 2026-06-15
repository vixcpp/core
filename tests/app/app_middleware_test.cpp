/**
 *
 * @file app_middleware_test.cpp
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
#include <string>
#include <type_traits>
#include <vector>

#include <vix/app/App.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/router/Router.hpp>

namespace
{
  using App = vix::App;
  using Request = vix::http::Request;
  using ResponseWrapper = vix::http::ResponseWrapper;
  using RouteRecord = vix::router::Router::RouteRecord;

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

  static void text_handler(Request &, ResponseWrapper &res)
  {
    res.ok().text("ok");
  }

  static App::Middleware make_passthrough_middleware()
  {
    return [](Request &, ResponseWrapper &, App::Next next)
    {
      next();
    };
  }

  static App::Middleware make_blocking_middleware()
  {
    return [](Request &, ResponseWrapper &res, App::Next)
    {
      res.status(401).text("blocked");
    };
  }

  static bool has_record(
      const std::vector<RouteRecord> &records,
      const std::string &method,
      const std::string &path,
      bool heavy = false)
  {
    for (const auto &record : records)
    {
      if (record.method == method &&
          record.path == path &&
          record.heavy == heavy)
      {
        return true;
      }
    }

    return false;
  }

  static std::size_t count_records(
      const std::vector<RouteRecord> &records,
      const std::string &method,
      const std::string &path)
  {
    std::size_t count = 0;

    for (const auto &record : records)
    {
      if (record.method == method &&
          record.path == path)
      {
        ++count;
      }
    }

    return count;
  }

  static void assert_route_registered(
      App &app,
      const std::string &method,
      const std::string &path)
  {
    assert(app.router() != nullptr);
    assert(app.router()->has_route(method, path) == true);
  }

  static void assert_route_not_registered(
      App &app,
      const std::string &method,
      const std::string &path)
  {
    assert(app.router() != nullptr);
    assert(app.router()->has_route(method, path) == false);
  }

  static void test_middleware_type_is_callable()
  {
    static_assert(std::is_copy_constructible_v<App::Middleware>);
    static_assert(std::is_move_constructible_v<App::Middleware>);
    static_assert(std::is_copy_assignable_v<App::Middleware>);
    static_assert(std::is_move_assignable_v<App::Middleware>);

    static_assert(std::is_copy_constructible_v<App::Next>);
    static_assert(std::is_move_constructible_v<App::Next>);
  }

  static void test_global_use_accepts_passthrough_middleware()
  {
    prepare_app_env();

    App app;

    app.use(make_passthrough_middleware());

    app.get("/status", text_handler);

    assert_route_registered(app, "GET", "/status");
    assert_route_registered(app, "OPTIONS", "/status");

    app.close();
  }

  static void test_global_use_accepts_inline_lambda()
  {
    prepare_app_env();

    App app;

    bool executed = false;

    app.use(
        [&executed](Request &, ResponseWrapper &, App::Next next)
        {
          executed = true;
          next();
        });

    /*
     * Registration alone must not execute middleware.
     */
    assert(executed == false);

    app.get("/inline", text_handler);

    assert(executed == false);
    assert_route_registered(app, "GET", "/inline");
    assert_route_registered(app, "OPTIONS", "/inline");

    app.close();
  }

  static void test_global_use_accepts_blocking_middleware()
  {
    prepare_app_env();

    App app;

    app.use(make_blocking_middleware());

    app.get("/blocked", text_handler);

    assert_route_registered(app, "GET", "/blocked");
    assert_route_registered(app, "OPTIONS", "/blocked");

    app.close();
  }

  static void test_prefix_use_accepts_passthrough_middleware()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());

    app.get("/api/status", text_handler);
    app.get("/public/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    assert_route_registered(app, "GET", "/public/status");
    assert_route_registered(app, "OPTIONS", "/public/status");

    app.close();
  }

  static void test_prefix_use_accepts_prefix_without_leading_slash()
  {
    prepare_app_env();

    App app;

    app.use("api", make_passthrough_middleware());

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_prefix_use_accepts_prefix_with_trailing_slash()
  {
    prepare_app_env();

    App app;

    app.use("/api/", make_passthrough_middleware());

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_prefix_use_accepts_root_prefix()
  {
    prepare_app_env();

    App app;

    app.use("/", make_passthrough_middleware());

    app.get("/root-prefix", text_handler);

    assert_route_registered(app, "GET", "/root-prefix");
    assert_route_registered(app, "OPTIONS", "/root-prefix");

    app.close();
  }

  static void test_prefix_use_accepts_empty_prefix()
  {
    prepare_app_env();

    App app;

    app.use("", make_passthrough_middleware());

    app.get("/empty-prefix", text_handler);

    assert_route_registered(app, "GET", "/empty-prefix");
    assert_route_registered(app, "OPTIONS", "/empty-prefix");

    app.close();
  }

  static void test_multiple_global_middlewares_can_be_registered()
  {
    prepare_app_env();

    App app;

    bool first_executed = false;
    bool second_executed = false;
    bool third_executed = false;

    app.use(
        [&first_executed](Request &, ResponseWrapper &, App::Next next)
        {
          first_executed = true;
          next();
        });

    app.use(
        [&second_executed](Request &, ResponseWrapper &, App::Next next)
        {
          second_executed = true;
          next();
        });

    app.use(
        [&third_executed](Request &, ResponseWrapper &, App::Next next)
        {
          third_executed = true;
          next();
        });

    assert(first_executed == false);
    assert(second_executed == false);
    assert(third_executed == false);

    app.get("/many-middleware", text_handler);

    assert(first_executed == false);
    assert(second_executed == false);
    assert(third_executed == false);

    assert_route_registered(app, "GET", "/many-middleware");
    assert_route_registered(app, "OPTIONS", "/many-middleware");

    app.close();
  }

  static void test_multiple_prefix_middlewares_can_be_registered()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());
    app.use("/api/private", make_passthrough_middleware());
    app.use("/admin", make_passthrough_middleware());

    app.get("/api/status", text_handler);
    app.get("/api/private/me", text_handler);
    app.get("/admin/dashboard", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "GET", "/admin/dashboard");

    assert_route_registered(app, "OPTIONS", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/admin/dashboard");

    app.close();
  }

  static void test_protect_accepts_prefix_middleware()
  {
    prepare_app_env();

    App app;

    app.protect("/private", make_blocking_middleware());

    app.get("/private/me", text_handler);
    app.get("/public/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    assert_route_registered(app, "GET", "/public/me");
    assert_route_registered(app, "OPTIONS", "/public/me");

    app.close();
  }

  static void test_protect_accepts_prefix_without_leading_slash()
  {
    prepare_app_env();

    App app;

    app.protect("private", make_blocking_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_accepts_prefix_with_trailing_slash()
  {
    prepare_app_env();

    App app;

    app.protect("/private/", make_blocking_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_exact_accepts_exact_path_middleware()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/private/me", make_blocking_middleware());

    app.get("/private/me", text_handler);
    app.get("/private/me/details", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    assert_route_registered(app, "GET", "/private/me/details");
    assert_route_registered(app, "OPTIONS", "/private/me/details");

    app.close();
  }

  static void test_protect_exact_accepts_path_without_leading_slash()
  {
    prepare_app_env();

    App app;

    app.protect_exact("private/me", make_blocking_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_exact_accepts_trailing_slash_path()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/private/me/", make_blocking_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_use_and_protect_can_be_combined()
  {
    prepare_app_env();

    App app;

    app.use(make_passthrough_middleware());
    app.use("/api", make_passthrough_middleware());
    app.protect("/api/private", make_blocking_middleware());
    app.protect_exact("/api/private/me", make_passthrough_middleware());

    app.get("/api/status", text_handler);
    app.get("/api/private/me", text_handler);
    app.get("/api/private/settings", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "GET", "/api/private/settings");

    assert_route_registered(app, "OPTIONS", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/settings");

    app.close();
  }

  static void test_middleware_registration_before_routes()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());

    app.get("/api/one", text_handler);
    app.post("/api/two", text_handler);
    app.put("/api/three", text_handler);
    app.patch("/api/four", text_handler);
    app.del("/api/five", text_handler);

    assert_route_registered(app, "GET", "/api/one");
    assert_route_registered(app, "POST", "/api/two");
    assert_route_registered(app, "PUT", "/api/three");
    assert_route_registered(app, "PATCH", "/api/four");
    assert_route_registered(app, "DELETE", "/api/five");

    app.close();
  }

  static void test_middleware_registration_after_routes_keeps_existing_routes()
  {
    prepare_app_env();

    App app;

    app.get("/api/one", text_handler);
    app.post("/api/two", text_handler);

    assert_route_registered(app, "GET", "/api/one");
    assert_route_registered(app, "POST", "/api/two");

    app.use("/api", make_passthrough_middleware());
    app.protect("/api/private", make_blocking_middleware());

    assert_route_registered(app, "GET", "/api/one");
    assert_route_registered(app, "POST", "/api/two");

    app.get("/api/private/three", text_handler);

    assert_route_registered(app, "GET", "/api/private/three");
    assert_route_registered(app, "OPTIONS", "/api/private/three");

    app.close();
  }

  static void test_middleware_registration_does_not_add_routes()
  {
    prepare_app_env();

    App app;

    const std::size_t before = app.router()->routes().size();

    app.use(make_passthrough_middleware());
    app.use("/api", make_passthrough_middleware());
    app.protect("/private", make_blocking_middleware());
    app.protect_exact("/private/me", make_blocking_middleware());

    const std::size_t after = app.router()->routes().size();

    assert(before == after);

    assert_route_not_registered(app, "GET", "/api");
    assert_route_not_registered(app, "GET", "/private");
    assert_route_not_registered(app, "GET", "/private/me");

    app.close();
  }

  static void test_middleware_registration_does_not_start_server()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.use(make_passthrough_middleware());
    app.use("/api", make_passthrough_middleware());
    app.protect("/private", make_blocking_middleware());
    app.protect_exact("/private/me", make_blocking_middleware());

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_middleware_registration_does_not_change_dev_mode()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    app.use(make_passthrough_middleware());
    app.use("/api", make_passthrough_middleware());
    app.protect("/private", make_blocking_middleware());
    app.protect_exact("/private/me", make_blocking_middleware());

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_middleware_registration_does_not_change_config()
  {
    prepare_app_env();

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    app.use(make_passthrough_middleware());
    app.use("/api", make_passthrough_middleware());
    app.protect("/private", make_blocking_middleware());
    app.protect_exact("/private/me", make_blocking_middleware());

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    app.close();
  }

  static void test_middleware_registration_after_templates()
  {
    prepare_app_env();

    App app;

    app.templates("views");

    assert(app.has_views() == true);

    app.use(make_passthrough_middleware());
    app.use("/api", make_passthrough_middleware());

    app.get("/api/status", text_handler);

    assert(app.has_views() == true);
    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_group_use_registers_group_prefixed_middleware()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.use(make_passthrough_middleware());
          api.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_group_use_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.use(make_passthrough_middleware());

    assert(&returned == &api);

    returned.get("/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_group_protect_registers_group_prefixed_middleware()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.protect("/private", make_blocking_middleware());
          api.get("/private/me", text_handler);
          api.get("/public/me", text_handler);
        });

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    assert_route_registered(app, "GET", "/api/public/me");
    assert_route_registered(app, "OPTIONS", "/api/public/me");

    app.close();
  }

  static void test_group_protect_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.protect("/private", make_blocking_middleware());

    assert(&returned == &api);

    returned.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_group_protect_exact_registers_group_prefixed_exact_middleware()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.protect_exact("/private/me", make_blocking_middleware());
          api.get("/private/me", text_handler);
          api.get("/private/me/details", text_handler);
        });

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    assert_route_registered(app, "GET", "/api/private/me/details");
    assert_route_registered(app, "OPTIONS", "/api/private/me/details");

    app.close();
  }

  static void test_group_protect_exact_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.protect_exact("/private/me", make_blocking_middleware());

    assert(&returned == &api);

    returned.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_group_middleware_helpers_can_be_chained()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    api.use(make_passthrough_middleware())
        .protect("/private", make_blocking_middleware())
        .protect_exact("/private/me", make_passthrough_middleware())
        .get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_nested_group_middleware_helpers()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.use(make_passthrough_middleware());

          api.group(
              "/v1",
              [](App::Group v1)
              {
                v1.use(make_passthrough_middleware());
                v1.protect("/private", make_blocking_middleware());
                v1.protect_exact("/private/me", make_passthrough_middleware());

                v1.get("/private/me", text_handler);
                v1.get("/public/status", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/v1/private/me");
    assert_route_registered(app, "OPTIONS", "/api/v1/private/me");

    assert_route_registered(app, "GET", "/api/v1/public/status");
    assert_route_registered(app, "OPTIONS", "/api/v1/public/status");

    app.close();
  }

  static void test_group_middleware_prefix_normalization()
  {
    prepare_app_env();

    App app;

    app.group(
        "api/",
        [](App::Group api)
        {
          api.use(make_passthrough_middleware());
          api.protect("private/", make_blocking_middleware());
          api.protect_exact("private/me/", make_passthrough_middleware());

          api.group(
              "v1/",
              [](App::Group v1)
              {
                v1.use(make_passthrough_middleware());
                v1.get("status", text_handler);
              });

          api.get("private/me", text_handler);
        });

    assert_route_registered(app, "GET", "/api/v1/status");
    assert_route_registered(app, "OPTIONS", "/api/v1/status");

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_middleware_with_heavy_routes()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());

    app.get_heavy("/api/reports", text_handler);
    app.post_heavy("/api/reports", text_handler);

    assert_route_registered(app, "GET", "/api/reports");
    assert_route_registered(app, "POST", "/api/reports");
    assert_route_registered(app, "OPTIONS", "/api/reports");

    Request get_req{
        std::string{"GET"},
        std::string{"/api/reports"}};

    Request post_req{
        std::string{"POST"},
        std::string{"/api/reports"}};

    Request options_req{
        std::string{"OPTIONS"},
        std::string{"/api/reports"}};

    assert(app.router()->is_heavy(get_req) == true);
    assert(app.router()->is_heavy(post_req) == true);
    assert(app.router()->is_heavy(options_req) == false);

    app.close();
  }

  static void test_middleware_with_param_routes()
  {
    prepare_app_env();

    App app;

    app.use("/api/users", make_passthrough_middleware());
    app.protect("/api/users/{id}/private", make_blocking_middleware());

    app.get("/api/users/{id}", text_handler);
    app.get("/api/users/{id}/private/profile", text_handler);

    assert_route_registered(app, "GET", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/42");

    assert_route_registered(app, "GET", "/api/users/42/private/profile");
    assert_route_registered(app, "OPTIONS", "/api/users/42/private/profile");

    app.close();
  }

  static void test_middleware_with_duplicate_route_registration()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());

    app.get("/api/duplicate", text_handler);
    app.get("/api/duplicate", text_handler);

    assert_route_registered(app, "GET", "/api/duplicate");
    assert_route_registered(app, "OPTIONS", "/api/duplicate");

    const auto &records = app.router()->routes();

    assert(count_records(records, "GET", "/api/duplicate") == 2u);
    assert(count_records(records, "OPTIONS", "/api/duplicate") == 1u);

    app.close();
  }

  static void test_middleware_with_manual_options_route()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());

    app.options("/api/preflight", text_handler);
    app.get("/api/preflight", text_handler);

    assert_route_registered(app, "OPTIONS", "/api/preflight");
    assert_route_registered(app, "GET", "/api/preflight");

    const auto &records = app.router()->routes();

    assert(count_records(records, "OPTIONS", "/api/preflight") == 1u);
    assert(count_records(records, "GET", "/api/preflight") == 1u);

    app.close();
  }

  static void test_many_middlewares_and_routes_can_be_registered()
  {
    prepare_app_env();

    App app;

    for (int i = 0; i < 20; ++i)
    {
      app.use(
          "/api/" + std::to_string(i),
          make_passthrough_middleware());

      app.protect(
          "/api/" + std::to_string(i) + "/private",
          make_blocking_middleware());
    }

    for (int i = 0; i < 20; ++i)
    {
      app.get(
          "/api/" + std::to_string(i) + "/status",
          text_handler);

      app.get(
          "/api/" + std::to_string(i) + "/private/me",
          text_handler);
    }

    for (int i = 0; i < 20; ++i)
    {
      assert_route_registered(
          app,
          "GET",
          "/api/" + std::to_string(i) + "/status");

      assert_route_registered(
          app,
          "OPTIONS",
          "/api/" + std::to_string(i) + "/status");

      assert_route_registered(
          app,
          "GET",
          "/api/" + std::to_string(i) + "/private/me");

      assert_route_registered(
          app,
          "OPTIONS",
          "/api/" + std::to_string(i) + "/private/me");
    }

    app.close();
  }

  static void test_middleware_routes_survive_close_before_listen()
  {
    prepare_app_env();

    App app;

    app.use("/api", make_passthrough_middleware());
    app.protect("/api/private", make_blocking_middleware());

    app.get("/api/status", text_handler);
    app.get("/api/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "GET", "/api/private/me");

    app.close();

    assert(app.is_running() == false);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");
  }

  static void test_middleware_contract_can_mutate_response_headers()
  {
    Request req{
        std::string{"GET"},
        std::string{"/state"}};

    vix::http::Response response;
    ResponseWrapper res{response};

    bool next_called = false;

    App::Middleware middleware =
        [](Request &, ResponseWrapper &output, App::Next next)
    {
      output.header("X-Middleware", "enabled");
      next();
    };

    App::Next next =
        [&next_called]()
    {
      next_called = true;
    };

    middleware(req, res, next);

    assert(next_called == true);
    assert(response.header("X-Middleware") == "enabled");
  }

  static void test_blocking_middleware_contract_can_skip_next()
  {
    Request req{
        std::string{"GET"},
        std::string{"/blocked"}};

    vix::http::Response response;
    ResponseWrapper res{response};

    bool next_called = false;

    App::Middleware middleware =
        [](Request &, ResponseWrapper &output, App::Next)
    {
      output.status(403).text("forbidden");
    };

    App::Next next =
        [&next_called]()
    {
      next_called = true;
    };

    middleware(req, res, next);

    assert(next_called == false);
    assert(response.status() == vix::http::FORBIDDEN);
    assert(response.body() == "forbidden");
  }

  static void test_passthrough_middleware_contract_can_call_next_once()
  {
    Request req{
        std::string{"GET"},
        std::string{"/next"}};

    vix::http::Response response;
    ResponseWrapper res{response};

    int next_calls = 0;

    App::Middleware middleware =
        [](Request &, ResponseWrapper &, App::Next next)
    {
      next();
    };

    App::Next next =
        [&next_calls]()
    {
      ++next_calls;
    };

    middleware(req, res, next);

    assert(next_calls == 1);
  }

} // namespace

int main()
{
  test_middleware_type_is_callable();

  test_global_use_accepts_passthrough_middleware();
  test_global_use_accepts_inline_lambda();
  test_global_use_accepts_blocking_middleware();

  test_prefix_use_accepts_passthrough_middleware();
  test_prefix_use_accepts_prefix_without_leading_slash();
  test_prefix_use_accepts_prefix_with_trailing_slash();
  test_prefix_use_accepts_root_prefix();
  test_prefix_use_accepts_empty_prefix();

  test_multiple_global_middlewares_can_be_registered();
  test_multiple_prefix_middlewares_can_be_registered();

  test_protect_accepts_prefix_middleware();
  test_protect_accepts_prefix_without_leading_slash();
  test_protect_accepts_prefix_with_trailing_slash();

  test_protect_exact_accepts_exact_path_middleware();
  test_protect_exact_accepts_path_without_leading_slash();
  test_protect_exact_accepts_trailing_slash_path();

  test_use_and_protect_can_be_combined();

  test_middleware_registration_before_routes();
  test_middleware_registration_after_routes_keeps_existing_routes();

  test_middleware_registration_does_not_add_routes();
  test_middleware_registration_does_not_start_server();
  test_middleware_registration_does_not_change_dev_mode();
  test_middleware_registration_does_not_change_config();
  test_middleware_registration_after_templates();

  test_group_use_registers_group_prefixed_middleware();
  test_group_use_returns_group_reference();

  test_group_protect_registers_group_prefixed_middleware();
  test_group_protect_returns_group_reference();

  test_group_protect_exact_registers_group_prefixed_exact_middleware();
  test_group_protect_exact_returns_group_reference();

  test_group_middleware_helpers_can_be_chained();
  test_nested_group_middleware_helpers();
  test_group_middleware_prefix_normalization();

  test_middleware_with_heavy_routes();
  test_middleware_with_param_routes();
  test_middleware_with_duplicate_route_registration();
  test_middleware_with_manual_options_route();

  test_many_middlewares_and_routes_can_be_registered();
  test_middleware_routes_survive_close_before_listen();

  test_middleware_contract_can_mutate_response_headers();
  test_blocking_middleware_contract_can_skip_next();
  test_passthrough_middleware_contract_can_call_next_once();

  return 0;
}
