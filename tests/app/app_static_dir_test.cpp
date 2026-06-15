/**
 *
 * @file app_static_dir_test.cpp
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
#include <string>

#include <vix/app/App.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>

namespace
{
  using App = vix::App;
  using Request = vix::http::Request;
  using ResponseWrapper = vix::http::ResponseWrapper;

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

  static void write_file(
      const std::filesystem::path &path,
      const std::string &content)
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

  static void text_handler(Request &, ResponseWrapper &res)
  {
    res.ok().text("ok");
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

  static App &register_static_dir(
      App &app,
      const std::filesystem::path &root,
      const std::string &mount = "/")
  {
    std::filesystem::path mutable_root = root;

    app.static_dir(mutable_root, mount);

    return app;
  }

  static App &register_static_dir(
      App &app,
      const std::filesystem::path &root,
      const std::string &mount,
      const std::string &index_file,
      bool add_cache_control,
      const std::string &cache_control,
      bool fallthrough)
  {
    std::filesystem::path mutable_root = root;

    app.static_dir(
        mutable_root,
        mount,
        index_file,
        add_cache_control,
        cache_control,
        fallthrough);

    return app;
  }

  static void test_static_dir_returns_app_reference()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_return_ref");

    App app;

    App &returned = register_static_dir(app, public_dir);

    assert(&returned == &app);

    app.close();
  }

  static void test_static_dir_with_default_mount_accepts_existing_empty_directory()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_empty_default_mount");

    App app;

    register_static_dir(app, public_dir);

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();
  }

  static void test_static_dir_with_default_mount_accepts_existing_files()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_files_default_mount");

    write_file(public_dir / "index.html", "<html>home</html>");
    write_file(public_dir / "app.css", "body { margin: 0; }");
    write_file(public_dir / "app.js", "console.log('vix');");

    App app;

    register_static_dir(app, public_dir);

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_with_nested_files()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_nested_files");

    write_file(public_dir / "assets" / "css" / "app.css", "body{}");
    write_file(public_dir / "assets" / "js" / "app.js", "console.log(1);");
    write_file(public_dir / "images" / "logo.txt", "logo");

    App app;

    register_static_dir(app, public_dir);

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_with_custom_mount()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_custom_mount");

    write_file(public_dir / "index.html", "assets");

    App app;

    register_static_dir(app, public_dir, "/assets");

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();
  }

  static void test_static_dir_mount_without_leading_slash_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_mount_no_slash");

    write_file(public_dir / "index.html", "assets");

    App app;

    register_static_dir(app, public_dir, "assets");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_mount_with_trailing_slash_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_mount_trailing_slash");

    write_file(public_dir / "index.html", "assets");

    App app;

    register_static_dir(app, public_dir, "/assets/");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_root_mount_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_root_mount");

    write_file(public_dir / "index.html", "root");

    App app;

    register_static_dir(app, public_dir, "/");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_empty_mount_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_empty_mount");

    write_file(public_dir / "index.html", "root");

    App app;

    register_static_dir(app, public_dir, "");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_missing_directory_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_missing_root");

    const std::filesystem::path missing_dir = root / "missing-public";

    assert(std::filesystem::exists(missing_dir) == false);

    App app;

    register_static_dir(app, missing_dir, "/assets");

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();
  }

  static void test_static_dir_relative_path_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_relative_root");

    const auto old_cwd = std::filesystem::current_path();

    std::filesystem::current_path(root);

    std::error_code ec;
    std::filesystem::create_directories("public", ec);
    assert(!ec);

    write_file("public/index.html", "relative");

    App app;

    const std::filesystem::path relative_public_dir{"public"};

    register_static_dir(app, relative_public_dir, "/assets");

    assert(app.is_running() == false);

    app.close();

    std::filesystem::current_path(old_cwd);
  }

  static void test_static_dir_relative_missing_path_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_relative_missing_root");

    const auto old_cwd = std::filesystem::current_path();

    std::filesystem::current_path(root);

    assert(std::filesystem::exists("missing-public") == false);

    App app;

    const std::filesystem::path relative_missing_dir{"missing-public"};

    register_static_dir(app, relative_missing_dir, "/assets");

    assert(app.is_running() == false);

    app.close();

    std::filesystem::current_path(old_cwd);
  }

  static void test_static_dir_absolute_path_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_absolute");

    write_file(public_dir / "index.html", "absolute");

    App app;

    const std::filesystem::path absolute_public_dir =
        std::filesystem::absolute(public_dir);

    register_static_dir(app, absolute_public_dir, "/static");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_can_be_called_more_than_once()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_multiple_public");

    const std::filesystem::path assets_dir =
        make_temp_dir("vix_app_static_multiple_assets");

    write_file(public_dir / "index.html", "public");
    write_file(assets_dir / "app.css", "body{}");

    App app;

    App &first = register_static_dir(app, public_dir, "/");
    App &second = register_static_dir(app, assets_dir, "/assets");

    assert(&first == &app);
    assert(&second == &app);

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_same_mount_can_be_registered_more_than_once()
  {
    prepare_app_env();

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_static_same_mount_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_static_same_mount_second");

    write_file(first_dir / "index.html", "first");
    write_file(second_dir / "index.html", "second");

    App app;

    register_static_dir(app, first_dir, "/assets");
    register_static_dir(app, second_dir, "/assets");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_different_mounts_can_be_registered()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_diff_public");

    const std::filesystem::path assets_dir =
        make_temp_dir("vix_app_static_diff_assets");

    const std::filesystem::path downloads_dir =
        make_temp_dir("vix_app_static_diff_downloads");

    write_file(public_dir / "index.html", "public");
    write_file(assets_dir / "app.css", "assets");
    write_file(downloads_dir / "readme.txt", "downloads");

    App app;

    register_static_dir(app, public_dir, "/");
    register_static_dir(app, assets_dir, "/assets");
    register_static_dir(app, downloads_dir, "/downloads");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_does_not_register_router_route_by_itself()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_no_router_route");

    write_file(public_dir / "index.html", "home");

    App app;

    const std::size_t before = app.router()->routes().size();

    register_static_dir(app, public_dir, "/assets");

    const std::size_t after = app.router()->routes().size();

    assert(before == after);

    assert_route_not_registered(app, "GET", "/assets");
    assert_route_not_registered(app, "GET", "/assets/index.html");
    assert_route_not_registered(app, "OPTIONS", "/assets/index.html");

    app.close();
  }

  static void test_static_dir_before_route_registration_keeps_route_registration_valid()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_before_route");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/assets");

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_static_dir_after_route_registration_keeps_existing_routes()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_after_route");

    write_file(public_dir / "index.html", "home");

    App app;

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    register_static_dir(app, public_dir, "/assets");

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_static_dir_with_group_routes()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_group_routes");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/assets");

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
          api.post("/users", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    assert_route_registered(app, "POST", "/api/users");
    assert_route_registered(app, "OPTIONS", "/api/users");

    app.close();
  }

  static void test_static_dir_with_protected_routes()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_protected_routes");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/assets");

    app.protect(
        "/api/private",
        [](Request &, ResponseWrapper &, App::Next next)
        {
          next();
        });

    app.get("/api/private/me", text_handler);

    assert_route_registered(app, "GET", "/api/private/me");
    assert_route_registered(app, "OPTIONS", "/api/private/me");

    app.close();
  }

  static void test_static_dir_with_middleware()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_middleware");

    write_file(public_dir / "index.html", "home");

    App app;

    app.use(
        [](Request &, ResponseWrapper &, App::Next next)
        {
          next();
        });

    register_static_dir(app, public_dir, "/assets");

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_static_dir_with_templates()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_templates_root");

    const std::filesystem::path public_dir = root / "public";
    const std::filesystem::path views_dir = root / "views";

    write_file(public_dir / "index.html", "home");
    write_file(views_dir / "index.html", "<html>{{ title }}</html>");

    App app;

    app.templates(views_dir.string());

    assert(app.has_views() == true);

    register_static_dir(app, public_dir, "/assets");

    assert(app.has_views() == true);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_after_templates()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_after_templates_root");

    const std::filesystem::path public_dir = root / "public";
    const std::filesystem::path views_dir = root / "views";

    write_file(public_dir / "index.html", "home");
    write_file(views_dir / "index.html", "<html>{{ title }}</html>");

    App app;

    app.templates(views_dir.string());
    register_static_dir(app, public_dir, "/assets");

    assert(app.has_views() == true);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_before_templates()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_before_templates_root");

    const std::filesystem::path public_dir = root / "public";
    const std::filesystem::path views_dir = root / "views";

    write_file(public_dir / "index.html", "home");
    write_file(views_dir / "index.html", "<html>{{ title }}</html>");

    App app;

    register_static_dir(app, public_dir, "/assets");
    app.templates(views_dir.string());

    assert(app.has_views() == true);
    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_does_not_start_server()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_no_server");

    write_file(public_dir / "index.html", "home");

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    register_static_dir(app, public_dir, "/assets");

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

  static void test_static_dir_does_not_change_dev_mode()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_dev_mode");

    write_file(public_dir / "index.html", "home");

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    register_static_dir(app, public_dir, "/assets");

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    app.close();
  }

  static void test_static_dir_does_not_change_config()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_config");

    write_file(public_dir / "index.html", "home");

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    register_static_dir(app, public_dir, "/assets");

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    app.close();
  }

  static void test_static_dir_survives_request_stop_from_signal_before_listen()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_signal");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/assets");

    assert(app.is_running() == false);

    app.request_stop_from_signal();

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_survives_close_before_listen()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_close");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/assets");

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();

    assert(app.is_running() == false);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_multiple_apps_have_independent_static_dir_registration()
  {
    prepare_app_env();

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_static_multi_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_static_multi_second");

    write_file(first_dir / "index.html", "first");
    write_file(second_dir / "index.html", "second");

    App first;
    App second;

    register_static_dir(first, first_dir, "/assets");
    register_static_dir(second, second_dir, "/public");

    first.get("/first", text_handler);
    second.get("/second", text_handler);

    assert_route_registered(first, "GET", "/first");
    assert_route_not_registered(first, "GET", "/second");

    assert_route_registered(second, "GET", "/second");
    assert_route_not_registered(second, "GET", "/first");

    first.close();
    second.close();
  }

  static void test_static_dir_before_multiple_route_registration()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_chain");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/assets");
    app.get("/api/status", text_handler);
    app.post("/api/users", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    assert_route_registered(app, "POST", "/api/users");
    assert_route_registered(app, "OPTIONS", "/api/users");

    app.close();
  }

  static void test_static_dir_before_templates_and_route_registration()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_chain_templates");

    const std::filesystem::path public_dir = root / "public";
    const std::filesystem::path views_dir = root / "views";

    write_file(public_dir / "index.html", "home");
    write_file(views_dir / "index.html", "<html>{{ title }}</html>");

    App app;

    register_static_dir(app, public_dir, "/assets");
    app.templates(views_dir.string());
    app.get("/api/status", text_handler);

    assert(app.has_views() == true);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_static_dir_can_register_many_directories()
  {
    prepare_app_env();

    App app;

    constexpr int count = 20;

    for (int i = 0; i < count; ++i)
    {
      const std::filesystem::path dir =
          make_temp_dir("vix_app_static_many_" + std::to_string(i));

      write_file(dir / "index.html", "dir-" + std::to_string(i));

      register_static_dir(
          app,
          dir,
          "/assets/" + std::to_string(i));
    }

    assert(app.is_running() == false);

    app.get("/api/status", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();
  }

  static void test_static_dir_file_paths_with_spaces_are_accepted()
  {
    prepare_app_env();

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_spaces_root");

    const std::filesystem::path public_dir = root / "public assets";

    write_file(public_dir / "hello world.txt", "hello");

    App app;

    register_static_dir(app, public_dir, "/assets");

    assert(app.is_running() == false);

    app.close();
  }

  static void test_static_dir_mount_with_spaces_is_accepted()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_mount_spaces");

    write_file(public_dir / "index.html", "home");

    App app;

    register_static_dir(app, public_dir, "/my assets");

    assert(app.is_running() == false);

    app.close();
  }

} // namespace

int main()
{
  test_static_dir_returns_app_reference();

  test_static_dir_with_default_mount_accepts_existing_empty_directory();
  test_static_dir_with_default_mount_accepts_existing_files();
  test_static_dir_with_nested_files();

  test_static_dir_with_custom_mount();
  test_static_dir_mount_without_leading_slash_is_accepted();
  test_static_dir_mount_with_trailing_slash_is_accepted();
  test_static_dir_root_mount_is_accepted();
  test_static_dir_empty_mount_is_accepted();

  test_static_dir_missing_directory_is_accepted();

  test_static_dir_relative_path_is_accepted();
  test_static_dir_relative_missing_path_is_accepted();
  test_static_dir_absolute_path_is_accepted();

  test_static_dir_can_be_called_more_than_once();
  test_static_dir_same_mount_can_be_registered_more_than_once();
  test_static_dir_different_mounts_can_be_registered();

  test_static_dir_does_not_register_router_route_by_itself();

  test_static_dir_before_route_registration_keeps_route_registration_valid();
  test_static_dir_after_route_registration_keeps_existing_routes();

  test_static_dir_with_group_routes();
  test_static_dir_with_protected_routes();
  test_static_dir_with_middleware();

  test_static_dir_with_templates();
  test_static_dir_after_templates();
  test_static_dir_before_templates();

  test_static_dir_does_not_start_server();
  test_static_dir_does_not_change_dev_mode();
  test_static_dir_does_not_change_config();

  test_static_dir_survives_request_stop_from_signal_before_listen();
  test_static_dir_survives_close_before_listen();

  test_multiple_apps_have_independent_static_dir_registration();

  test_static_dir_before_multiple_route_registration();
  test_static_dir_before_templates_and_route_registration();

  test_static_dir_can_register_many_directories();

  test_static_dir_file_paths_with_spaces_are_accepted();
  test_static_dir_mount_with_spaces_is_accepted();

  return 0;
}
