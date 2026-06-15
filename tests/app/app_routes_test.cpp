/**
 *
 * @file app_routes_test.cpp
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
      bool heavy)
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

  static std::size_t count_route_records(const App &app)
  {
    assert(app.router() != nullptr);
    return app.router()->routes().size();
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

  static void test_constructor_registers_bench_route()
  {
    prepare_app_env();

    App app;

    assert_route_registered(app, "GET", "/bench");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/bench", false) == true);

    app.close();
  }

  static void test_get_registers_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.get("/hello", text_handler);

    assert_route_registered(app, "GET", "/hello");
    assert_route_registered(app, "OPTIONS", "/hello");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 2u);
    assert(has_record(records, "GET", "/hello", false) == true);
    assert(has_record(records, "OPTIONS", "/hello", false) == true);

    app.close();
  }

  static void test_post_registers_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.post("/submit", text_handler);

    assert_route_registered(app, "POST", "/submit");
    assert_route_registered(app, "OPTIONS", "/submit");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 2u);
    assert(has_record(records, "POST", "/submit", false) == true);
    assert(has_record(records, "OPTIONS", "/submit", false) == true);

    app.close();
  }

  static void test_put_registers_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.put("/users/{id}", text_handler);

    assert_route_registered(app, "PUT", "/users/123");
    assert_route_registered(app, "OPTIONS", "/users/123");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 2u);
    assert(has_record(records, "PUT", "/users/{id}", false) == true);
    assert(has_record(records, "OPTIONS", "/users/{id}", false) == true);

    app.close();
  }

  static void test_patch_registers_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.patch("/users/{id}", text_handler);

    assert_route_registered(app, "PATCH", "/users/123");
    assert_route_registered(app, "OPTIONS", "/users/123");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 2u);
    assert(has_record(records, "PATCH", "/users/{id}", false) == true);
    assert(has_record(records, "OPTIONS", "/users/{id}", false) == true);

    app.close();
  }

  static void test_delete_registers_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.del("/users/{id}", text_handler);

    assert_route_registered(app, "DELETE", "/users/123");
    assert_route_registered(app, "OPTIONS", "/users/123");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 2u);
    assert(has_record(records, "DELETE", "/users/{id}", false) == true);
    assert(has_record(records, "OPTIONS", "/users/{id}", false) == true);

    app.close();
  }

  static void test_head_registers_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.head("/ping", text_handler);

    assert_route_registered(app, "HEAD", "/ping");
    assert_route_registered(app, "OPTIONS", "/ping");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 2u);
    assert(has_record(records, "HEAD", "/ping", false) == true);
    assert(has_record(records, "OPTIONS", "/ping", false) == true);

    app.close();
  }

  static void test_options_registers_only_options_route()
  {
    prepare_app_env();

    App app;

    const std::size_t before = count_route_records(app);

    app.options("/manual-options", text_handler);

    assert_route_registered(app, "OPTIONS", "/manual-options");

    assert_route_not_registered(app, "GET", "/manual-options");
    assert_route_not_registered(app, "POST", "/manual-options");
    assert_route_not_registered(app, "PUT", "/manual-options");
    assert_route_not_registered(app, "PATCH", "/manual-options");
    assert_route_not_registered(app, "DELETE", "/manual-options");

    const auto &records = app.router()->routes();

    assert(records.size() == before + 1u);
    assert(has_record(records, "OPTIONS", "/manual-options", false) == true);
    assert(count_records(records, "OPTIONS", "/manual-options") == 1u);

    app.close();
  }

  static void test_multiple_methods_can_share_same_path()
  {
    prepare_app_env();

    App app;

    app.get("/resource", text_handler);
    app.post("/resource", text_handler);
    app.put("/resource", text_handler);
    app.patch("/resource", text_handler);
    app.del("/resource", text_handler);
    app.head("/resource", text_handler);

    assert_route_registered(app, "GET", "/resource");
    assert_route_registered(app, "POST", "/resource");
    assert_route_registered(app, "PUT", "/resource");
    assert_route_registered(app, "PATCH", "/resource");
    assert_route_registered(app, "DELETE", "/resource");
    assert_route_registered(app, "HEAD", "/resource");

    /*
     * The first non-OPTIONS registration installs OPTIONS.
     * Later methods reuse the same auto OPTIONS route.
     */
    assert_route_registered(app, "OPTIONS", "/resource");

    const auto &records = app.router()->routes();

    assert(count_records(records, "OPTIONS", "/resource") == 1u);

    app.close();
  }

  static void test_get_heavy_registers_heavy_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    app.get_heavy("/heavy-read", text_handler);

    assert_route_registered(app, "GET", "/heavy-read");
    assert_route_registered(app, "OPTIONS", "/heavy-read");

    Request get_req{
        std::string{"GET"},
        std::string{"/heavy-read"}};

    Request options_req{
        std::string{"OPTIONS"},
        std::string{"/heavy-read"}};

    assert(app.router()->is_heavy(get_req) == true);
    assert(app.router()->is_heavy(options_req) == false);

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/heavy-read", true) == true);
    assert(has_record(records, "OPTIONS", "/heavy-read", false) == true);

    app.close();
  }

  static void test_post_heavy_registers_heavy_route_and_auto_options()
  {
    prepare_app_env();

    App app;

    app.post_heavy("/heavy-write", text_handler);

    assert_route_registered(app, "POST", "/heavy-write");
    assert_route_registered(app, "OPTIONS", "/heavy-write");

    Request post_req{
        std::string{"POST"},
        std::string{"/heavy-write"}};

    Request options_req{
        std::string{"OPTIONS"},
        std::string{"/heavy-write"}};

    assert(app.router()->is_heavy(post_req) == true);
    assert(app.router()->is_heavy(options_req) == false);

    const auto &records = app.router()->routes();

    assert(has_record(records, "POST", "/heavy-write", true) == true);
    assert(has_record(records, "OPTIONS", "/heavy-write", false) == true);

    app.close();
  }

  static void test_normal_get_route_is_not_heavy()
  {
    prepare_app_env();

    App app;

    app.get("/light", text_handler);

    Request req{
        std::string{"GET"},
        std::string{"/light"}};

    assert(app.router()->is_heavy(req) == false);

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/light", false) == true);

    app.close();
  }

  static void test_normal_post_route_is_not_heavy()
  {
    prepare_app_env();

    App app;

    app.post("/light-write", text_handler);

    Request req{
        std::string{"POST"},
        std::string{"/light-write"}};

    assert(app.router()->is_heavy(req) == false);

    const auto &records = app.router()->routes();

    assert(has_record(records, "POST", "/light-write", false) == true);

    app.close();
  }

  static void test_path_without_leading_slash_is_normalized_by_router()
  {
    prepare_app_env();

    App app;

    app.get("no-leading-slash", text_handler);

    assert_route_registered(app, "GET", "/no-leading-slash");
    assert_route_registered(app, "GET", "no-leading-slash");

    assert_route_registered(app, "OPTIONS", "/no-leading-slash");
    assert_route_registered(app, "OPTIONS", "no-leading-slash");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/no-leading-slash", false) == true);
    assert(has_record(records, "OPTIONS", "/no-leading-slash", false) == true);

    app.close();
  }

  static void test_trailing_slash_is_normalized_by_router()
  {
    prepare_app_env();

    App app;

    app.get("/trailing/", text_handler);

    assert_route_registered(app, "GET", "/trailing");
    assert_route_registered(app, "GET", "/trailing/");

    assert_route_registered(app, "OPTIONS", "/trailing");
    assert_route_registered(app, "OPTIONS", "/trailing/");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/trailing", false) == true);
    assert(has_record(records, "OPTIONS", "/trailing", false) == true);

    app.close();
  }

  static void test_empty_path_is_normalized_to_root()
  {
    prepare_app_env();

    App app;

    app.get("", text_handler);

    assert_route_registered(app, "GET", "/");
    assert_route_registered(app, "OPTIONS", "/");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/", false) == true);
    assert(has_record(records, "OPTIONS", "/", false) == true);

    app.close();
  }

  static void test_root_path_registration()
  {
    prepare_app_env();

    App app;

    app.get("/", text_handler);

    assert_route_registered(app, "GET", "/");
    assert_route_registered(app, "OPTIONS", "/");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/", false) == true);
    assert(has_record(records, "OPTIONS", "/", false) == true);

    app.close();
  }

  static void test_query_string_is_ignored_by_has_route()
  {
    prepare_app_env();

    App app;

    app.get("/search", text_handler);

    assert_route_registered(app, "GET", "/search");
    assert_route_registered(app, "GET", "/search?q=vix");
    assert_route_registered(app, "OPTIONS", "/search?q=vix");

    app.close();
  }

  static void test_param_route_matches_concrete_path()
  {
    prepare_app_env();

    App app;

    app.get("/users/{id}", text_handler);

    assert_route_registered(app, "GET", "/users/1");
    assert_route_registered(app, "GET", "/users/abc");
    assert_route_registered(app, "OPTIONS", "/users/1");

    assert_route_not_registered(app, "GET", "/users");
    assert_route_not_registered(app, "GET", "/users/1/profile");

    app.close();
  }

  static void test_nested_param_route_matches_concrete_path()
  {
    prepare_app_env();

    App app;

    app.get("/orgs/{org}/users/{user}", text_handler);

    assert_route_registered(app, "GET", "/orgs/vix/users/gaspard");
    assert_route_registered(app, "OPTIONS", "/orgs/vix/users/gaspard");

    assert_route_not_registered(app, "GET", "/orgs/vix/users");
    assert_route_not_registered(app, "GET", "/orgs/vix/users/gaspard/extra");

    app.close();
  }

  static void test_static_route_preferred_over_param_route()
  {
    prepare_app_env();

    App app;

    app.get("/users/{id}", text_handler);
    app.get("/users/me", text_handler);

    assert_route_registered(app, "GET", "/users/123");
    assert_route_registered(app, "GET", "/users/me");

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/users/{id}", false) == true);
    assert(has_record(records, "GET", "/users/me", false) == true);

    app.close();
  }

  static void test_route_records_preserve_normalized_methods()
  {
    prepare_app_env();

    App app;

    app.get("/a", text_handler);
    app.post("/b", text_handler);
    app.put("/c", text_handler);
    app.patch("/d", text_handler);
    app.del("/e", text_handler);
    app.head("/f", text_handler);
    app.options("/g", text_handler);

    const auto &records = app.router()->routes();

    assert(has_record(records, "GET", "/a", false) == true);
    assert(has_record(records, "POST", "/b", false) == true);
    assert(has_record(records, "PUT", "/c", false) == true);
    assert(has_record(records, "PATCH", "/d", false) == true);
    assert(has_record(records, "DELETE", "/e", false) == true);
    assert(has_record(records, "HEAD", "/f", false) == true);
    assert(has_record(records, "OPTIONS", "/g", false) == true);

    app.close();
  }

  static void test_route_registration_does_not_start_server()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.get("/one", text_handler);
    app.post("/two", text_handler);
    app.get_heavy("/three", text_handler);

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_route_registration_does_not_change_dev_mode()
  {
    prepare_app_env();

    App app;

    assert(app.isDevMode() == false);

    app.setDevMode(true);

    app.get("/dev", text_handler);
    app.post("/dev", text_handler);

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    assert_route_registered(app, "GET", "/dev");
    assert_route_registered(app, "POST", "/dev");
    assert_route_registered(app, "OPTIONS", "/dev");

    app.close();
  }

  static void test_route_registration_does_not_change_config()
  {
    prepare_app_env();

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    app.get("/config", text_handler);

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    assert_route_registered(app, "GET", "/config");
    assert_route_registered(app, "OPTIONS", "/config");

    app.close();
  }

  static void test_route_registration_after_templates()
  {
    prepare_app_env();

    App app;

    app.templates("views");

    assert(app.has_views() == true);

    app.get("/with-views", text_handler);

    assert_route_registered(app, "GET", "/with-views");
    assert_route_registered(app, "OPTIONS", "/with-views");

    app.close();
  }

  static void test_routes_survive_close_before_listen()
  {
    prepare_app_env();

    App app;

    app.get("/before-close", text_handler);

    assert_route_registered(app, "GET", "/before-close");
    assert_route_registered(app, "OPTIONS", "/before-close");

    app.close();

    assert(app.is_running() == false);
    assert_route_registered(app, "GET", "/before-close");
    assert_route_registered(app, "OPTIONS", "/before-close");
  }

  static void test_many_routes_can_be_registered()
  {
    prepare_app_env();

    App app;

    constexpr int count = 50;

    for (int i = 0; i < count; ++i)
    {
      app.get(
          "/items/" + std::to_string(i),
          text_handler);
    }

    for (int i = 0; i < count; ++i)
    {
      const std::string path = "/items/" + std::to_string(i);

      assert_route_registered(app, "GET", path);
      assert_route_registered(app, "OPTIONS", path);
    }

    app.close();
  }

  static void test_duplicate_route_registration_keeps_route_available()
  {
    prepare_app_env();

    App app;

    app.get("/duplicate", text_handler);
    app.get("/duplicate", text_handler);

    assert_route_registered(app, "GET", "/duplicate");
    assert_route_registered(app, "OPTIONS", "/duplicate");

    const auto &records = app.router()->routes();

    assert(count_records(records, "GET", "/duplicate") == 2u);

    /*
     * OPTIONS is auto-installed once and reused by the second GET registration.
     */
    assert(count_records(records, "OPTIONS", "/duplicate") == 1u);

    app.close();
  }

  static void test_manual_options_before_get_prevents_auto_options_duplicate()
  {
    prepare_app_env();

    App app;

    app.options("/preflight", text_handler);
    app.get("/preflight", text_handler);

    assert_route_registered(app, "OPTIONS", "/preflight");
    assert_route_registered(app, "GET", "/preflight");

    const auto &records = app.router()->routes();

    assert(count_records(records, "OPTIONS", "/preflight") == 1u);
    assert(count_records(records, "GET", "/preflight") == 1u);

    app.close();
  }

  static void test_get_before_manual_options_allows_manual_options_replacement_record()
  {
    prepare_app_env();

    App app;

    app.get("/preflight-after", text_handler);
    app.options("/preflight-after", text_handler);

    assert_route_registered(app, "GET", "/preflight-after");
    assert_route_registered(app, "OPTIONS", "/preflight-after");

    const auto &records = app.router()->routes();

    /*
     * GET installs an automatic OPTIONS route first. A later explicit OPTIONS
     * registration is also recorded by the router.
     */
    assert(count_records(records, "GET", "/preflight-after") == 1u);
    assert(count_records(records, "OPTIONS", "/preflight-after") == 2u);

    app.close();
  }

  static void test_heavy_route_with_param_matches_concrete_request()
  {
    prepare_app_env();

    App app;

    app.get_heavy("/reports/{id}", text_handler);

    Request req{
        std::string{"GET"},
        std::string{"/reports/42"}};

    Request query_req{
        std::string{"GET"},
        std::string{"/reports/42?format=json"}};

    assert_route_registered(app, "GET", "/reports/42");
    assert_route_registered(app, "OPTIONS", "/reports/42");

    assert(app.router()->is_heavy(req) == true);
    assert(app.router()->is_heavy(query_req) == true);

    app.close();
  }

  static void test_unregistered_route_is_not_heavy()
  {
    prepare_app_env();

    App app;

    app.get_heavy("/registered-heavy", text_handler);

    Request missing_req{
        std::string{"GET"},
        std::string{"/missing-heavy"}};

    assert(app.router()->is_heavy(missing_req) == false);

    app.close();
  }

} // namespace

int main()
{
  test_constructor_registers_bench_route();

  test_get_registers_route_and_auto_options();
  test_post_registers_route_and_auto_options();
  test_put_registers_route_and_auto_options();
  test_patch_registers_route_and_auto_options();
  test_delete_registers_route_and_auto_options();
  test_head_registers_route_and_auto_options();

  test_options_registers_only_options_route();

  test_multiple_methods_can_share_same_path();

  test_get_heavy_registers_heavy_route_and_auto_options();
  test_post_heavy_registers_heavy_route_and_auto_options();
  test_normal_get_route_is_not_heavy();
  test_normal_post_route_is_not_heavy();

  test_path_without_leading_slash_is_normalized_by_router();
  test_trailing_slash_is_normalized_by_router();
  test_empty_path_is_normalized_to_root();
  test_root_path_registration();
  test_query_string_is_ignored_by_has_route();

  test_param_route_matches_concrete_path();
  test_nested_param_route_matches_concrete_path();
  test_static_route_preferred_over_param_route();

  test_route_records_preserve_normalized_methods();

  test_route_registration_does_not_start_server();
  test_route_registration_does_not_change_dev_mode();
  test_route_registration_does_not_change_config();
  test_route_registration_after_templates();

  test_routes_survive_close_before_listen();

  test_many_routes_can_be_registered();

  test_duplicate_route_registration_keeps_route_available();
  test_manual_options_before_get_prevents_auto_options_duplicate();
  test_get_before_manual_options_allows_manual_options_replacement_record();

  test_heavy_route_with_param_matches_concrete_request();
  test_unregistered_route_is_not_heavy();

  return 0;
}
