/**
 *
 * @file app_group_test.cpp
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
#include <iostream>

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

    if (!app.router()->has_route(method, path))
    {
      std::cerr
          << "missing route: " << method << " " << path << "\n";

      for (const auto &record : app.router()->routes())
      {
        std::cerr
            << "registered: "
            << record.method
            << " "
            << record.path
            << " heavy="
            << (record.heavy ? "true" : "false")
            << "\n";
      }
    }

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

  static void test_group_get_registers_prefixed_route()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_group_post_registers_prefixed_route()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.post("/users", text_handler);
        });

    assert_route_registered(app, "POST", "/api/users");
    assert_route_registered(app, "OPTIONS", "/api/users");

    const auto &records = app.router()->routes();

    assert(has_record(records, "POST", "/api/users") == true);
    assert(has_record(records, "OPTIONS", "/api/users") == true);

    app.close();
  }

  static void test_group_put_registers_prefixed_route()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.put("/users/{id}", text_handler);
        });

    assert_route_registered(app, "PUT", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/42");

    const auto &records = app.router()->routes();

    assert(has_record(records, "PUT", "/api/users/{id}") == true);
    assert(has_record(records, "OPTIONS", "/api/users/{id}") == true);

    app.close();
  }

  static void test_group_patch_registers_prefixed_route()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.patch("/users/{id}", text_handler);
        });

    assert_route_registered(app, "PATCH", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/42");

    const auto &records = app.router()->routes();

    assert(has_record(records, "PATCH", "/api/users/{id}") == true);
    assert(has_record(records, "OPTIONS", "/api/users/{id}") == true);

    app.close();
  }

  static void test_group_delete_registers_prefixed_route()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.del("/users/{id}", text_handler);
        });

    assert_route_registered(app, "DELETE", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/42");

    const auto &records = app.router()->routes();

    assert(has_record(records, "DELETE", "/api/users/{id}") == true);
    assert(has_record(records, "OPTIONS", "/api/users/{id}") == true);

    app.close();
  }

  static void test_group_can_register_multiple_methods_on_same_path()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get("/users", text_handler);
          g.post("/users", text_handler);
          g.put("/users", text_handler);
          g.patch("/users", text_handler);
          g.del("/users", text_handler);
        });

    assert_route_registered(app, "GET", "/api/users");
    assert_route_registered(app, "POST", "/api/users");
    assert_route_registered(app, "PUT", "/api/users");
    assert_route_registered(app, "PATCH", "/api/users");
    assert_route_registered(app, "DELETE", "/api/users");
    assert_route_registered(app, "OPTIONS", "/api/users");

    const auto &records = app.router()->routes();

    assert(count_records(records, "OPTIONS", "/api/users") == 1u);

    app.close();
  }

  static void test_returned_group_object_can_register_routes()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    api.get("/status", text_handler);
    api.post("/users", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    assert_route_registered(app, "POST", "/api/users");
    assert_route_registered(app, "OPTIONS", "/api/users");

    app.close();
  }

  static void test_returned_group_object_can_be_reused()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    api.get("/one", text_handler);
    api.get("/two", text_handler);
    api.get("/three", text_handler);

    assert_route_registered(app, "GET", "/api/one");
    assert_route_registered(app, "GET", "/api/two");
    assert_route_registered(app, "GET", "/api/three");

    assert_route_registered(app, "OPTIONS", "/api/one");
    assert_route_registered(app, "OPTIONS", "/api/two");
    assert_route_registered(app, "OPTIONS", "/api/three");

    app.close();
  }

  static void test_group_prefix_without_leading_slash_is_normalized()
  {
    prepare_app_env();

    App app;

    app.group(
        "api",
        [](App::Group g)
        {
          g.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_group_prefix_trailing_slash_is_trimmed()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api/",
        [](App::Group g)
        {
          g.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "GET", "/api/status/");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_group_route_without_leading_slash_is_normalized()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get("status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_group_route_trailing_slash_is_trimmed()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get("/status/", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "GET", "/api/status/");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_empty_group_prefix_behaves_like_root()
  {
    prepare_app_env();

    App app;

    app.group(
        "",
        [](App::Group g)
        {
          g.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/status");
    assert_route_registered(app, "OPTIONS", "/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/status") == true);
    assert(has_record(records, "OPTIONS", "/status") == true);

    app.close();
  }

  static void test_root_group_prefix_preserves_root_join_behavior()
  {
    prepare_app_env();

    App app;

    app.group(
        "/",
        [](App::Group g)
        {
          g.get("/status", text_handler);
        });

    /*
     * Current Group::join behavior preserves the root slash and the child
     * slash, so this registers //status.
     */
    assert_route_registered(app, "GET", "//status");
    assert_route_registered(app, "OPTIONS", "//status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "//status") == true);
    assert(has_record(records, "OPTIONS", "//status") == true);

    app.close();
  }

  static void test_empty_route_path_inside_group_registers_group_root()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get("", text_handler);
        });

    assert_route_registered(app, "GET", "/api");
    assert_route_registered(app, "GET", "/api/");
    assert_route_registered(app, "OPTIONS", "/api");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api") == true);
    assert(has_record(records, "OPTIONS", "/api") == true);

    app.close();
  }

  static void test_slash_route_path_inside_group_registers_group_root()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group g)
        {
          g.get("/", text_handler);
        });

    assert_route_registered(app, "GET", "/api");
    assert_route_registered(app, "GET", "/api/");
    assert_route_registered(app, "OPTIONS", "/api");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api") == true);
    assert(has_record(records, "OPTIONS", "/api") == true);

    app.close();
  }

  static void test_empty_group_and_empty_route_registers_root()
  {
    prepare_app_env();

    App app;

    app.group(
        "",
        [](App::Group g)
        {
          g.get("", text_handler);
        });

    assert_route_registered(app, "GET", "/");
    assert_route_registered(app, "OPTIONS", "/");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/") == true);
    assert(has_record(records, "OPTIONS", "/") == true);

    app.close();
  }

  static void test_root_group_and_empty_route_registers_root()
  {
    prepare_app_env();

    App app;

    app.group(
        "/",
        [](App::Group g)
        {
          g.get("", text_handler);
        });

    assert_route_registered(app, "GET", "/");
    assert_route_registered(app, "OPTIONS", "/");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/") == true);
    assert(has_record(records, "OPTIONS", "/") == true);

    app.close();
  }

  static void test_nested_group_registers_joined_path()
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
                v1.get("/status", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/v1/status");
    assert_route_registered(app, "OPTIONS", "/api/v1/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/v1/status") == true);
    assert(has_record(records, "OPTIONS", "/api/v1/status") == true);

    app.close();
  }

  static void test_nested_group_normalizes_missing_slashes()
  {
    prepare_app_env();

    App app;

    app.group(
        "api",
        [](App::Group api)
        {
          api.group(
              "v1",
              [](App::Group v1)
              {
                v1.get("status", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/v1/status");
    assert_route_registered(app, "OPTIONS", "/api/v1/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/v1/status") == true);
    assert(has_record(records, "OPTIONS", "/api/v1/status") == true);

    app.close();
  }

  static void test_nested_group_trims_trailing_slashes()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api/",
        [](App::Group api)
        {
          api.group(
              "/v1/",
              [](App::Group v1)
              {
                v1.get("/status/", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/v1/status");
    assert_route_registered(app, "GET", "/api/v1/status/");
    assert_route_registered(app, "OPTIONS", "/api/v1/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/v1/status") == true);
    assert(has_record(records, "OPTIONS", "/api/v1/status") == true);

    app.close();
  }

  static void test_deeply_nested_groups_register_joined_path()
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
                v1.group(
                    "/admin",
                    [](App::Group admin)
                    {
                      admin.group(
                          "/reports",
                          [](App::Group reports)
                          {
                            reports.get("/{id}", text_handler);
                          });
                    });
              });
        });

    assert_route_registered(app, "GET", "/api/v1/admin/reports/42");
    assert_route_registered(app, "OPTIONS", "/api/v1/admin/reports/42");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/v1/admin/reports/{id}") == true);
    assert(has_record(records, "OPTIONS", "/api/v1/admin/reports/{id}") == true);

    app.close();
  }

  static void test_nested_group_empty_subprefix_keeps_parent_prefix()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.group(
              "",
              [](App::Group nested)
              {
                nested.get("/status", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_nested_group_root_subprefix_keeps_parent_prefix()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.group(
              "/",
              [](App::Group nested)
              {
                nested.get("/status", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/status") == true);
    assert(has_record(records, "OPTIONS", "/api/status") == true);

    app.close();
  }

  static void test_group_param_route_matches_concrete_path()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/users/{id}", text_handler);
        });

    assert_route_registered(app, "GET", "/api/users/1");
    assert_route_registered(app, "GET", "/api/users/gaspard");
    assert_route_registered(app, "OPTIONS", "/api/users/1");

    assert_route_not_registered(app, "GET", "/api/users");
    assert_route_not_registered(app, "GET", "/api/users/1/profile");

    app.close();
  }

  static void test_group_nested_param_route_matches_concrete_path()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.group(
              "/orgs/{org}",
              [](App::Group org)
              {
                org.get("/users/{user}", text_handler);
              });
        });

    assert_route_registered(app, "GET", "/api/orgs/vix/users/gaspard");
    assert_route_registered(app, "OPTIONS", "/api/orgs/vix/users/gaspard");

    assert_route_not_registered(app, "GET", "/api/orgs/vix/users");
    assert_route_not_registered(app, "GET", "/api/orgs/vix/users/gaspard/extra");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/orgs/{org}/users/{user}") == true);
    assert(has_record(records, "OPTIONS", "/api/orgs/{org}/users/{user}") == true);

    app.close();
  }

  static void test_group_query_string_is_ignored_by_route_match()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/search", text_handler);
        });

    assert_route_registered(app, "GET", "/api/search");
    assert_route_registered(app, "GET", "/api/search?q=vix");
    assert_route_registered(app, "OPTIONS", "/api/search?q=vix");

    app.close();
  }

  static void test_group_static_route_and_param_route_can_coexist()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/users/{id}", text_handler);
          api.get("/users/me", text_handler);
        });

    assert_route_registered(app, "GET", "/api/users/42");
    assert_route_registered(app, "GET", "/api/users/me");
    assert_route_registered(app, "OPTIONS", "/api/users/42");
    assert_route_registered(app, "OPTIONS", "/api/users/me");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/api/users/{id}") == true);
    assert(has_record(records, "GET", "/api/users/me") == true);

    app.close();
  }

  static void test_group_auto_options_is_installed_once_per_path()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/resource", text_handler);
          api.post("/resource", text_handler);
          api.put("/resource", text_handler);
          api.patch("/resource", text_handler);
          api.del("/resource", text_handler);
        });

    assert_route_registered(app, "OPTIONS", "/api/resource");

    const auto &records = app.router()->routes();

    assert(count_records(records, "OPTIONS", "/api/resource") == 1u);

    app.close();
  }

  static void test_group_duplicate_route_registration_keeps_route_available()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/duplicate", text_handler);
          api.get("/duplicate", text_handler);
        });

    assert_route_registered(app, "GET", "/api/duplicate");
    assert_route_registered(app, "OPTIONS", "/api/duplicate");

    const auto &records = app.router()->routes();

    assert(count_records(records, "GET", "/api/duplicate") == 2u);
    assert(count_records(records, "OPTIONS", "/api/duplicate") == 1u);

    app.close();
  }

  static void test_multiple_groups_with_different_prefixes_are_independent()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
        });

    app.group(
        "/admin",
        [](App::Group admin)
        {
          admin.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    assert_route_registered(app, "GET", "/admin/status");
    assert_route_registered(app, "OPTIONS", "/admin/status");

    assert_route_not_registered(app, "GET", "/status");

    app.close();
  }

  static void test_multiple_group_objects_with_same_prefix_can_register_routes()
  {
    prepare_app_env();

    App app;

    auto first = app.group("/api");
    auto second = app.group("/api");

    first.get("/one", text_handler);
    second.get("/two", text_handler);

    assert_route_registered(app, "GET", "/api/one");
    assert_route_registered(app, "OPTIONS", "/api/one");

    assert_route_registered(app, "GET", "/api/two");
    assert_route_registered(app, "OPTIONS", "/api/two");

    app.close();
  }

  static void test_group_registration_does_not_start_server()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
          api.post("/users", text_handler);
        });

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_group_registration_does_not_change_dev_mode()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
        });

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);
    assert_route_registered(app, "GET", "/api/status");

    app.close();
  }

  static void test_group_registration_does_not_change_config()
  {
    prepare_app_env();

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
        });

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    assert_route_registered(app, "GET", "/api/status");

    app.close();
  }

  static void test_group_registration_after_templates()
  {
    prepare_app_env();

    App app;

    app.templates("views");

    assert(app.has_views() == true);

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_group_routes_survive_close_before_listen()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();

    assert(app.is_running() == false);
    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");
  }

  static void test_group_use_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.use(
            [](Request &, ResponseWrapper &, App::Next next)
            {
              next();
            });

    assert(&returned == &api);

    api.get("/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_group_protect_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.protect(
            "/private",
            [](Request &, ResponseWrapper &, App::Next next)
            {
              next();
            });

    assert(&returned == &api);

    api.get("/private/status", text_handler);

    assert_route_registered(app, "GET", "/api/private/status");
    assert_route_registered(app, "OPTIONS", "/api/private/status");

    app.close();
  }

  static void test_group_protect_exact_returns_group_reference()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    App::Group &returned =
        api.protect_exact(
            "/private/status",
            [](Request &, ResponseWrapper &, App::Next next)
            {
              next();
            });

    assert(&returned == &api);

    api.get("/private/status", text_handler);

    assert_route_registered(app, "GET", "/api/private/status");
    assert_route_registered(app, "OPTIONS", "/api/private/status");

    app.close();
  }

  static void test_group_middleware_helpers_can_be_chained()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    api.use(
           [](Request &, ResponseWrapper &, App::Next next)
           {
             next();
           })
        .protect(
            "/private",
            [](Request &, ResponseWrapper &, App::Next next)
            {
              next();
            })
        .protect_exact(
            "/private/status",
            [](Request &, ResponseWrapper &, App::Next next)
            {
              next();
            });

    api.get("/private/status", text_handler);

    assert_route_registered(app, "GET", "/api/private/status");
    assert_route_registered(app, "OPTIONS", "/api/private/status");

    app.close();
  }

  static void test_group_many_routes_can_be_registered()
  {
    prepare_app_env();

    App app;

    auto api = app.group("/api");

    constexpr int count = 40;

    for (int i = 0; i < count; ++i)
    {
      api.get(
          "/items/" + std::to_string(i),
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

} // namespace

int main()
{
  test_group_get_registers_prefixed_route();
  test_group_post_registers_prefixed_route();
  test_group_put_registers_prefixed_route();
  test_group_patch_registers_prefixed_route();
  test_group_delete_registers_prefixed_route();

  test_group_can_register_multiple_methods_on_same_path();

  test_returned_group_object_can_register_routes();
  test_returned_group_object_can_be_reused();

  test_group_prefix_without_leading_slash_is_normalized();
  test_group_prefix_trailing_slash_is_trimmed();
  test_group_route_without_leading_slash_is_normalized();
  test_group_route_trailing_slash_is_trimmed();

  test_empty_group_prefix_behaves_like_root();
  test_root_group_prefix_preserves_root_join_behavior();

  test_empty_route_path_inside_group_registers_group_root();
  test_slash_route_path_inside_group_registers_group_root();
  test_empty_group_and_empty_route_registers_root();
  test_root_group_and_empty_route_registers_root();

  test_nested_group_registers_joined_path();
  test_nested_group_normalizes_missing_slashes();
  test_nested_group_trims_trailing_slashes();
  test_deeply_nested_groups_register_joined_path();
  test_nested_group_empty_subprefix_keeps_parent_prefix();
  test_nested_group_root_subprefix_keeps_parent_prefix();

  test_group_param_route_matches_concrete_path();
  test_group_nested_param_route_matches_concrete_path();
  test_group_query_string_is_ignored_by_route_match();
  test_group_static_route_and_param_route_can_coexist();

  test_group_auto_options_is_installed_once_per_path();
  test_group_duplicate_route_registration_keeps_route_available();

  test_multiple_groups_with_different_prefixes_are_independent();
  test_multiple_group_objects_with_same_prefix_can_register_routes();

  test_group_registration_does_not_start_server();
  test_group_registration_does_not_change_dev_mode();
  test_group_registration_does_not_change_config();
  test_group_registration_after_templates();

  test_group_routes_survive_close_before_listen();

  test_group_use_returns_group_reference();
  test_group_protect_returns_group_reference();
  test_group_protect_exact_returns_group_reference();
  test_group_middleware_helpers_can_be_chained();

  test_group_many_routes_can_be_registered();

  return 0;
}
