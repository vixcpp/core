/**
 *
 * @file app_protect_test.cpp
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
#include <vector>

#include <vix/app/App.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/router/Router.hpp>

namespace
{
  using App = vix::App;
  using Request = vix::http::Request;
  using Response = vix::http::Response;
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

  static App::Middleware allow_middleware()
  {
    return [](Request &, ResponseWrapper &, App::Next next)
    {
      next();
    };
  }

  static App::Middleware deny_middleware()
  {
    return [](Request &, ResponseWrapper &res, App::Next)
    {
      res.status(vix::http::FORBIDDEN).text("forbidden");
    };
  }

  static App::Middleware unauthorized_middleware()
  {
    return [](Request &, ResponseWrapper &res, App::Next)
    {
      res.status(vix::http::UNAUTHORIZED).text("unauthorized");
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

  static void test_protect_prefix_accepts_allow_middleware()
  {
    prepare_app_env();

    App app;

    app.protect("/private", allow_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_prefix_accepts_deny_middleware()
  {
    prepare_app_env();

    App app;

    app.protect("/private", deny_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_prefix_accepts_unauthorized_middleware()
  {
    prepare_app_env();

    App app;

    app.protect("/private", unauthorized_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_prefix_without_leading_slash_is_normalized()
  {
    prepare_app_env();

    App app;

    app.protect("private", allow_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_prefix_with_trailing_slash_is_normalized()
  {
    prepare_app_env();

    App app;

    app.protect("/private/", allow_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_root_prefix_is_allowed()
  {
    prepare_app_env();

    App app;

    app.protect("/", allow_middleware());

    app.get("/anything", text_handler);

    assert_route_registered(app, "GET", "/anything");
    assert_route_registered(app, "OPTIONS", "/anything");

    app.close();
  }

  static void test_protect_empty_prefix_is_allowed()
  {
    prepare_app_env();

    App app;

    app.protect("", allow_middleware());

    app.get("/anything", text_handler);

    assert_route_registered(app, "GET", "/anything");
    assert_route_registered(app, "OPTIONS", "/anything");

    app.close();
  }

  static void test_protect_exact_accepts_allow_middleware()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/private/me", allow_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_exact_accepts_deny_middleware()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/private/me", deny_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_exact_without_leading_slash_is_normalized()
  {
    prepare_app_env();

    App app;

    app.protect_exact("private/me", allow_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_exact_with_trailing_slash_is_normalized()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/private/me/", allow_middleware());

    app.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_exact_root_path_is_allowed()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/", allow_middleware());

    app.get("/", text_handler);

    assert_route_registered(app, "GET", "/");
    assert_route_registered(app, "OPTIONS", "/");

    app.close();
  }

  static void test_protect_does_not_add_routes_by_itself()
  {
    prepare_app_env();

    App app;

    const std::size_t before = app.router()->routes().size();

    app.protect("/private", allow_middleware());

    const std::size_t after = app.router()->routes().size();

    assert(before == after);

    assert_route_not_registered(app, "GET", "/private");
    assert_route_not_registered(app, "GET", "/private/me");
    assert_route_not_registered(app, "OPTIONS", "/private");

    app.close();
  }

  static void test_protect_exact_does_not_add_routes_by_itself()
  {
    prepare_app_env();

    App app;

    const std::size_t before = app.router()->routes().size();

    app.protect_exact("/private/me", allow_middleware());

    const std::size_t after = app.router()->routes().size();

    assert(before == after);

    assert_route_not_registered(app, "GET", "/private/me");
    assert_route_not_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protect_before_route_registration()
  {
    prepare_app_env();

    App app;

    app.protect("/api/private", allow_middleware());

    app.get("/api/private/me", text_handler);
    app.post("/api/private/login", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    assert_route_registered(app, "POST", "/api/private/login");
    assert_route_registered(app, "OPTIONS", "/api/private/login");

    app.close();
  }

  static void test_protect_after_route_registration_keeps_existing_routes()
  {
    prepare_app_env();

    App app;

    app.get("/api/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.protect("/api/private", allow_middleware());

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.post("/api/private/settings", text_handler);

    assert_route_registered(app, "POST", "/api/private/settings");
    assert_route_registered(app, "OPTIONS", "/api/private/settings");

    app.close();
  }

  static void test_protect_exact_before_route_registration()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/api/private/me", allow_middleware());

    app.get("/api/private/me", text_handler);
    app.get("/api/private/me/details", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    assert_route_registered(app, "GET", "/api/private/me/details");
    assert_route_registered(app, "OPTIONS", "/api/private/me/details");

    app.close();
  }

  static void test_protect_exact_after_route_registration_keeps_existing_routes()
  {
    prepare_app_env();

    App app;

    app.get("/api/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.protect_exact("/api/private/me", allow_middleware());

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_protect_and_protect_exact_can_be_combined()
  {
    prepare_app_env();

    App app;

    app.protect("/api/private", allow_middleware());
    app.protect_exact("/api/private/me", deny_middleware());

    app.get("/api/private/me", text_handler);
    app.get("/api/private/settings", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    assert_route_registered(app, "GET", "/api/private/settings");
    assert_route_registered(app, "OPTIONS", "/api/private/settings");

    app.close();
  }

  static void test_multiple_protect_prefixes_can_be_registered()
  {
    prepare_app_env();

    App app;

    app.protect("/api/private", allow_middleware());
    app.protect("/admin", deny_middleware());
    app.protect("/billing", unauthorized_middleware());

    app.get("/api/private/me", text_handler);
    app.get("/admin/dashboard", text_handler);
    app.get("/billing/invoices", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "GET", "/admin/dashboard");
    assert_route_registered(app, "GET", "/billing/invoices");

    assert_route_registered(app, "OPTIONS", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/admin/dashboard");
    assert_route_registered(app, "OPTIONS", "/billing/invoices");

    app.close();
  }

  static void test_multiple_protect_exact_paths_can_be_registered()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/private/me", allow_middleware());
    app.protect_exact("/admin/dashboard", deny_middleware());
    app.protect_exact("/billing/invoices", unauthorized_middleware());

    app.get("/private/me", text_handler);
    app.get("/admin/dashboard", text_handler);
    app.get("/billing/invoices", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "GET", "/admin/dashboard");
    assert_route_registered(app, "GET", "/billing/invoices");

    assert_route_registered(app, "OPTIONS", "/private/me");
    assert_route_registered(app, "OPTIONS", "/admin/dashboard");
    assert_route_registered(app, "OPTIONS", "/billing/invoices");

    app.close();
  }

  static void test_protect_with_param_routes()
  {
    prepare_app_env();

    App app;

    app.protect("/api/users", allow_middleware());

    app.get("/api/users/{id}", text_handler);
    app.get("/api/users/{id}/profile", text_handler);

    assert_route_registered(app, "GET", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/42");

    assert_route_registered(app, "GET", "/api/users/42/profile");
    assert_route_registered(app, "OPTIONS", "/api/users/42/profile");

    app.close();
  }

  static void test_protect_exact_with_param_route_pattern()
  {
    prepare_app_env();

    App app;

    app.protect_exact("/api/users/{id}", allow_middleware());

    app.get("/api/users/{id}", text_handler);
    app.get("/api/users/{id}/profile", text_handler);

    assert_route_registered(app, "GET", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/42");

    assert_route_registered(app, "GET", "/api/users/42/profile");
    assert_route_registered(app, "OPTIONS", "/api/users/42/profile");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/users/{id}") == true);
    assert(has_record(records, "GET", "/api/users/{id}/profile") == true);

    app.close();
  }

  static void test_protect_with_heavy_get_route()
  {
    prepare_app_env();

    App app;

    app.protect("/reports", allow_middleware());

    app.get_heavy("/reports/{id}", text_handler);

    assert_route_registered(app, "GET", "/reports/42");
    assert_route_registered(app, "OPTIONS", "/reports/42");

    Request req{
        std::string{"GET"},
        std::string{"/reports/42"}};

    Request options_req{
        std::string{"OPTIONS"},
        std::string{"/reports/42"}};

    assert(app.router()->is_heavy(req) == true);
    assert(app.router()->is_heavy(options_req) == false);

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/reports/{id}", true) == true);
    assert(has_record(records, "OPTIONS", "/reports/{id}", false) == true);

    app.close();
  }

  static void test_protect_with_heavy_post_route()
  {
    prepare_app_env();

    App app;

    app.protect("/reports", allow_middleware());

    app.post_heavy("/reports", text_handler);

    assert_route_registered(app, "POST", "/reports");
    assert_route_registered(app, "OPTIONS", "/reports");

    Request req{
        std::string{"POST"},
        std::string{"/reports"}};

    assert(app.router()->is_heavy(req) == true);

    const auto &records = app.router()->routes();

    assert(has_record(records, "POST", "/reports", true) == true);
    assert(has_record(records, "OPTIONS", "/reports", false) == true);

    app.close();
  }

  static void test_group_protect_prefix()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.protect("/private", allow_middleware());
          api.get("/private/me", text_handler);
        });

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_group_protect_exact_path()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.protect_exact("/private/me", allow_middleware());
          api.get("/private/me", text_handler);
        });

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_group_protect_normalizes_paths()
  {
    prepare_app_env();

    App app;

    app.group(
        "api/",
        [](App::Group api)
        {
          api.protect("private/", allow_middleware());
          api.protect_exact("private/me/", deny_middleware());

          api.get("private/me", text_handler);
          api.get("private/settings", text_handler);
        });

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    assert_route_registered(app, "GET", "/api/private/settings");
    assert_route_registered(app, "OPTIONS", "/api/private/settings");

    app.close();
  }

  static void test_group_protect_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.protect("/private", allow_middleware());

    assert(&returned == &api);

    returned.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_group_protect_exact_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.protect_exact("/private/me", allow_middleware());

    assert(&returned == &api);

    returned.get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_group_protect_can_be_chained()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    api.protect("/private", allow_middleware())
        .protect_exact("/private/me", deny_middleware())
        .get("/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_nested_group_protect_prefix()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.group(
              "/v1",
              [](App::Group v1)
              {
                v1.protect("/private", allow_middleware());
                v1.get("/private/me", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/v1/private/me");
    assert_route_registered(app, "OPTIONS", "/api/v1/private/me");

    app.close();
  }

  static void test_nested_group_protect_exact_path()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.group(
              "/v1",
              [](App::Group v1)
              {
                v1.protect_exact("/private/me", allow_middleware());
                v1.get("/private/me", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/v1/private/me");
    assert_route_registered(app, "OPTIONS", "/api/v1/private/me");

    app.close();
  }

  static void test_protect_does_not_start_server()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.protect("/private", allow_middleware());
    app.protect_exact("/private/me", deny_middleware());

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.get("/private/me", text_handler);

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_protect_does_not_change_dev_mode()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    app.protect("/private", allow_middleware());
    app.protect_exact("/private/me", deny_middleware());

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_protect_does_not_change_config()
  {
    prepare_app_env();

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    app.protect("/private", allow_middleware());
    app.protect_exact("/private/me", deny_middleware());

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    app.close();
  }

  static void test_protect_after_templates()
  {
    prepare_app_env();

    App app;

    app.templates("views");

    assert(app.has_views() == true);

    app.protect("/private", allow_middleware());
    app.protect_exact("/private/me", deny_middleware());

    app.get("/private/me", text_handler);

    assert(app.has_views() == true);
    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    app.close();
  }

  static void test_protected_routes_survive_close_before_listen()
  {
    prepare_app_env();

    App app;

    app.protect("/private", allow_middleware());
    app.protect_exact("/private/me", deny_middleware());

    app.get("/private/me", text_handler);
    app.get("/private/settings", text_handler);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "GET", "/private/settings");

    app.close();

    assert(app.is_running() == false);

    assert_route_registered(app, "GET", "/private/me");
    assert_route_registered(app, "OPTIONS", "/private/me");

    assert_route_registered(app, "GET", "/private/settings");
    assert_route_registered(app, "OPTIONS", "/private/settings");
  }

  static void test_protect_with_duplicate_route_registration()
  {
    prepare_app_env();

    App app;

    app.protect("/private", allow_middleware());

    app.get("/private/duplicate", text_handler);
    app.get("/private/duplicate", text_handler);

    assert_route_registered(app, "GET", "/private/duplicate");
    assert_route_registered(app, "OPTIONS", "/private/duplicate");

    const auto &records = app.router()->routes();

    assert(count_records(records, "GET", "/private/duplicate") == 2u);
    assert(count_records(records, "OPTIONS", "/private/duplicate") == 1u);

    app.close();
  }

  static void test_protect_with_manual_options_before_get()
  {
    prepare_app_env();

    App app;

    app.protect("/private", allow_middleware());

    app.options("/private/preflight", text_handler);
    app.get("/private/preflight", text_handler);

    assert_route_registered(app, "OPTIONS", "/private/preflight");
    assert_route_registered(app, "GET", "/private/preflight");

    const auto &records = app.router()->routes();

    assert(count_records(records, "OPTIONS", "/private/preflight") == 1u);
    assert(count_records(records, "GET", "/private/preflight") == 1u);

    app.close();
  }

  static void test_protect_with_manual_options_after_get()
  {
    prepare_app_env();

    App app;

    app.protect("/private", allow_middleware());

    app.get("/private/preflight", text_handler);
    app.options("/private/preflight", text_handler);

    assert_route_registered(app, "GET", "/private/preflight");
    assert_route_registered(app, "OPTIONS", "/private/preflight");

    const auto &records = app.router()->routes();

    assert(count_records(records, "GET", "/private/preflight") == 1u);
    assert(count_records(records, "OPTIONS", "/private/preflight") == 2u);

    app.close();
  }

  static void test_many_protected_routes_can_be_registered()
  {
    prepare_app_env();

    App app;

    app.protect("/api", allow_middleware());
    app.protect_exact("/api/admin", deny_middleware());

    constexpr int count = 40;

    for (int i = 0; i < count; ++i)
    {
      app.get(
          "/api/items/" + std::to_string(i),
          text_handler);
    }

    for (int i = 0; i < count; ++i)
    {
      const std::string path = "/api/items/" + std::to_string(i);

      assert_route_registered(app, "GET", path);
      assert_route_registered(app, "OPTIONS", path);
    }

    app.close();
  }

  static void test_allow_middleware_contract_calls_next()
  {
    Request req{
        std::string{"GET"},
        std::string{"/private"}};

    Response response;
    ResponseWrapper res{response};

    bool next_called = false;

    App::Next next =
        [&next_called]()
    {
      next_called = true;
    };

    App::Middleware middleware = allow_middleware();

    middleware(req, res, next);

    assert(next_called == true);
  }

  static void test_deny_middleware_contract_skips_next_and_sets_response()
  {
    Request req{
        std::string{"GET"},
        std::string{"/private"}};

    Response response;
    ResponseWrapper res{response};

    bool next_called = false;

    App::Next next =
        [&next_called]()
    {
      next_called = true;
    };

    App::Middleware middleware = deny_middleware();

    middleware(req, res, next);

    assert(next_called == false);
    assert(response.status() == vix::http::FORBIDDEN);
    assert(response.body() == "forbidden");
  }

  static void test_unauthorized_middleware_contract_skips_next_and_sets_response()
  {
    Request req{
        std::string{"GET"},
        std::string{"/private"}};

    Response response;
    ResponseWrapper res{response};

    bool next_called = false;

    App::Next next =
        [&next_called]()
    {
      next_called = true;
    };

    App::Middleware middleware = unauthorized_middleware();

    middleware(req, res, next);

    assert(next_called == false);
    assert(response.status() == vix::http::UNAUTHORIZED);
    assert(response.body() == "unauthorized");
  }

  static void test_header_middleware_contract_sets_response_header()
  {
    Request req{
        std::string{"GET"},
        std::string{"/private"}};

    Response response;
    ResponseWrapper res{response};

    bool next_called = false;

    App::Next next =
        [&next_called]()
    {
      next_called = true;
    };

    App::Middleware middleware =
        [](Request &, ResponseWrapper &output, App::Next next)
    {
      output.header("X-Protected", "yes");
      next();
    };

    middleware(req, res, next);

    assert(next_called == true);
    assert(response.header("X-Protected") == "yes");
  }
} // namespace

int main()
{
  test_protect_prefix_accepts_allow_middleware();
  test_protect_prefix_accepts_deny_middleware();
  test_protect_prefix_accepts_unauthorized_middleware();

  test_protect_prefix_without_leading_slash_is_normalized();
  test_protect_prefix_with_trailing_slash_is_normalized();
  test_protect_root_prefix_is_allowed();
  test_protect_empty_prefix_is_allowed();

  test_protect_exact_accepts_allow_middleware();
  test_protect_exact_accepts_deny_middleware();
  test_protect_exact_without_leading_slash_is_normalized();
  test_protect_exact_with_trailing_slash_is_normalized();
  test_protect_exact_root_path_is_allowed();

  test_protect_does_not_add_routes_by_itself();
  test_protect_exact_does_not_add_routes_by_itself();

  test_protect_before_route_registration();
  test_protect_after_route_registration_keeps_existing_routes();

  test_protect_exact_before_route_registration();
  test_protect_exact_after_route_registration_keeps_existing_routes();

  test_protect_and_protect_exact_can_be_combined();

  test_multiple_protect_prefixes_can_be_registered();
  test_multiple_protect_exact_paths_can_be_registered();

  test_protect_with_param_routes();
  test_protect_exact_with_param_route_pattern();

  test_protect_with_heavy_get_route();
  test_protect_with_heavy_post_route();

  test_group_protect_prefix();
  test_group_protect_exact_path();
  test_group_protect_normalizes_paths();
  test_group_protect_returns_group_reference();
  test_group_protect_exact_returns_group_reference();
  test_group_protect_can_be_chained();

  test_nested_group_protect_prefix();
  test_nested_group_protect_exact_path();

  test_protect_does_not_start_server();
  test_protect_does_not_change_dev_mode();
  test_protect_does_not_change_config();
  test_protect_after_templates();

  test_protected_routes_survive_close_before_listen();

  test_protect_with_duplicate_route_registration();
  test_protect_with_manual_options_before_get();
  test_protect_with_manual_options_after_get();

  test_many_protected_routes_can_be_registered();

  test_allow_middleware_contract_calls_next();
  test_deny_middleware_contract_skips_next_and_sets_response();
  test_unauthorized_middleware_contract_skips_next_and_sets_response();
  test_header_middleware_contract_sets_response_header();

  return 0;
}
