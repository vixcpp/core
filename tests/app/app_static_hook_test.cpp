/**
 *
 * @file app_static_hook_test.cpp
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
#include <type_traits>

#include <vix/app/App.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>

namespace
{
  using App = vix::App;
  using Request = vix::http::Request;
  using Response = vix::http::Response;
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

    App::set_static_handler({});
    App::set_static_response_hook({});
  }

  static void reset_static_hooks()
  {
    App::set_static_handler({});
    App::set_static_response_hook({});
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

  struct StaticHandlerCall
  {
    bool called{false};
    App *app{nullptr};
    std::filesystem::path root{};
    std::string mount{};
    std::string index_file{};
    bool add_cache_control{false};
    std::string cache_control{};
    bool fallthrough{false};
    int count{0};
  };

  static void test_static_handler_type_traits()
  {
    static_assert(std::is_copy_constructible_v<App::StaticHandler>);
    static_assert(std::is_move_constructible_v<App::StaticHandler>);
    static_assert(std::is_copy_assignable_v<App::StaticHandler>);
    static_assert(std::is_move_assignable_v<App::StaticHandler>);

    static_assert(std::is_copy_constructible_v<App::StaticResponseHook>);
    static_assert(std::is_move_constructible_v<App::StaticResponseHook>);
    static_assert(std::is_copy_assignable_v<App::StaticResponseHook>);
    static_assert(std::is_move_assignable_v<App::StaticResponseHook>);
  }

  static void test_set_static_handler_accepts_empty_handler()
  {
    prepare_app_env();

    App::set_static_handler({});

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_empty_handler");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_set_static_response_hook_accepts_empty_hook()
  {
    prepare_app_env();

    App::set_static_response_hook({});

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_response_empty_hook");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/");

    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_is_called_by_static_dir()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &app,
            const std::filesystem::path &root,
            const std::string &mount,
            const std::string &index_file,
            bool add_cache_control,
            const std::string &cache_control,
            bool fallthrough) -> bool
        {
          call.called = true;
          call.app = &app;
          call.root = root;
          call.mount = mount;
          call.index_file = index_file;
          call.add_cache_control = add_cache_control;
          call.cache_control = cache_control;
          call.fallthrough = fallthrough;
          ++call.count;

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_called");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(call.called == true);
    assert(call.count == 1);
    assert(call.app == &app);
    assert(call.root == public_dir);
    assert(call.mount == "/assets");
    assert(call.index_file == "index.html");
    assert(call.add_cache_control == true);
    assert(call.cache_control == "public, max-age=3600");
    assert(call.fallthrough == true);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_receives_custom_static_dir_options()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &app,
            const std::filesystem::path &root,
            const std::string &mount,
            const std::string &index_file,
            bool add_cache_control,
            const std::string &cache_control,
            bool fallthrough) -> bool
        {
          call.called = true;
          call.app = &app;
          call.root = root;
          call.mount = mount;
          call.index_file = index_file;
          call.add_cache_control = add_cache_control;
          call.cache_control = cache_control;
          call.fallthrough = fallthrough;
          ++call.count;

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_custom_options");

    App app;

    app.static_dir(
        public_dir,
        "/assets",
        "home.html",
        false,
        "no-store",
        false);

    assert(call.called == true);
    assert(call.count == 1);
    assert(call.app == &app);
    assert(call.root == public_dir);
    assert(call.mount == "/assets");
    assert(call.index_file == "home.html");
    assert(call.add_cache_control == false);
    assert(call.cache_control == "no-store");
    assert(call.fallthrough == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_normalizes_mount_without_leading_slash()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &app,
            const std::filesystem::path &root,
            const std::string &mount,
            const std::string &index_file,
            bool add_cache_control,
            const std::string &cache_control,
            bool fallthrough) -> bool
        {
          call.called = true;
          call.app = &app;
          call.root = root;
          call.mount = mount;
          call.index_file = index_file;
          call.add_cache_control = add_cache_control;
          call.cache_control = cache_control;
          call.fallthrough = fallthrough;
          ++call.count;

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_mount_no_slash");

    App app;

    app.static_dir(public_dir, "assets");

    assert(call.called == true);
    assert(call.mount == "/assets");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_normalizes_mount_with_trailing_slash()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          call.called = true;
          call.mount = mount;
          ++call.count;

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_mount_trailing_slash");

    App app;

    app.static_dir(public_dir, "/assets/");

    assert(call.called == true);
    assert(call.count == 1);
    assert(call.mount == "/assets");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_normalizes_empty_mount_to_root()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          call.called = true;
          call.mount = mount;
          ++call.count;

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_empty_mount");

    App app;

    app.static_dir(public_dir, "");

    assert(call.called == true);
    assert(call.count == 1);
    assert(call.mount == "");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_normalizes_root_mount_to_root()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          call.called = true;
          call.mount = mount;
          ++call.count;

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_root_mount");

    App app;

    app.static_dir(public_dir, "/");

    assert(call.called == true);
    assert(call.count == 1);
    assert(call.mount == "/");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_return_true_consumes_static_dir_registration()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &app,
            const std::filesystem::path &root,
            const std::string &mount,
            const std::string &index_file,
            bool add_cache_control,
            const std::string &cache_control,
            bool fallthrough) -> bool
        {
          call.called = true;
          call.app = &app;
          call.root = root;
          call.mount = mount;
          call.index_file = index_file;
          call.add_cache_control = add_cache_control;
          call.cache_control = cache_control;
          call.fallthrough = fallthrough;
          ++call.count;

          return true;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_consume");

    App app;

    const std::size_t before = app.router()->routes().size();

    app.static_dir(public_dir, "/assets");

    const std::size_t after = app.router()->routes().size();

    assert(call.called == true);
    assert(call.count == 1);
    assert(before == after);
    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_return_false_allows_core_static_registration()
  {
    prepare_app_env();

    StaticHandlerCall call{};

    App::set_static_handler(
        [&call](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          call.called = true;
          ++call.count;
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_fallthrough");

    App app;

    const std::size_t before = app.router()->routes().size();

    app.static_dir(public_dir, "/assets");

    const std::size_t after = app.router()->routes().size();

    assert(call.called == true);
    assert(call.count == 1);

    /*
     * Core static mounts are not normal router route records.
     */
    assert(before == after);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_can_register_custom_route()
  {
    prepare_app_env();

    bool handler_called = false;

    App::set_static_handler(
        [&handler_called](
            App &app,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          handler_called = true;

          app.get(
              mount + "/__custom_static",
              [](Request &, ResponseWrapper &res)
              {
                res.ok().text("custom static");
              });

          return true;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_custom_route");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(handler_called == true);

    assert_route_registered(app, "GET", "/assets/__custom_static");
    assert_route_registered(app, "OPTIONS", "/assets/__custom_static");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_custom_route_can_normalize_mount_itself()
  {
    prepare_app_env();

    bool handler_called = false;

    App::set_static_handler(
        [&handler_called](
            App &app,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          handler_called = true;

          std::string normalized = mount;

          if (normalized.empty())
          {
            normalized = "/";
          }

          if (normalized.front() != '/')
          {
            normalized.insert(normalized.begin(), '/');
          }

          while (normalized.size() > 1 && normalized.back() == '/')
          {
            normalized.pop_back();
          }

          app.get(
              normalized + "/hook",
              [](Request &, ResponseWrapper &res)
              {
                res.ok().text("hook");
              });

          return true;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_normalized_route");

    App app;

    app.static_dir(public_dir, "assets/");

    assert(handler_called == true);

    assert_route_registered(app, "GET", "/assets/hook");
    assert_route_registered(app, "OPTIONS", "/assets/hook");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_can_be_replaced()
  {
    prepare_app_env();

    int first_calls = 0;
    int second_calls = 0;

    App::set_static_handler(
        [&first_calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++first_calls;
          return false;
        });

    App::set_static_handler(
        [&second_calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++second_calls;
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_replace");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(first_calls == 0);
    assert(second_calls == 1);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_can_be_cleared()
  {
    prepare_app_env();

    int calls = 0;

    App::set_static_handler(
        [&calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;
          return false;
        });

    App::set_static_handler({});

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_clear");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(calls == 0);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_is_global_across_app_instances()
  {
    prepare_app_env();

    int calls = 0;
    App *first_seen = nullptr;
    App *second_seen = nullptr;

    App::set_static_handler(
        [&calls, &first_seen, &second_seen](
            App &app,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;

          if (calls == 1)
          {
            first_seen = &app;
          }
          else if (calls == 2)
          {
            second_seen = &app;
          }

          return false;
        });

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_static_hook_global_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_static_hook_global_second");

    App first;
    App second;

    first.static_dir(first_dir, "/first");
    second.static_dir(second_dir, "/second");

    assert(calls == 2);
    assert(first_seen == &first);
    assert(second_seen == &second);
    assert(first_seen != second_seen);

    first.close();
    second.close();

    reset_static_hooks();
  }

  static void test_static_handler_receives_each_static_dir_call()
  {
    prepare_app_env();

    int calls = 0;
    std::string first_mount;
    std::string second_mount;
    std::string third_mount;

    App::set_static_handler(
        [&calls, &first_mount, &second_mount, &third_mount](
            App &,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;

          if (calls == 1)
          {
            first_mount = mount;
          }
          else if (calls == 2)
          {
            second_mount = mount;
          }
          else if (calls == 3)
          {
            third_mount = mount;
          }

          return false;
        });

    const std::filesystem::path first_dir =
        make_temp_dir("vix_app_static_hook_each_first");

    const std::filesystem::path second_dir =
        make_temp_dir("vix_app_static_hook_each_second");

    const std::filesystem::path third_dir =
        make_temp_dir("vix_app_static_hook_each_third");

    App app;

    app.static_dir(first_dir, "/one");
    app.static_dir(second_dir, "/two");
    app.static_dir(third_dir, "/three");

    assert(calls == 3);
    assert(first_mount == "/one");
    assert(second_mount == "/two");
    assert(third_mount == "/three");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_does_not_start_server()
  {
    prepare_app_env();

    int calls = 0;

    App::set_static_handler(
        [&calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_no_server");

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.static_dir(public_dir, "/assets");

    assert(calls == 1);
    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_does_not_change_dev_mode()
  {
    prepare_app_env();

    App::set_static_handler(
        [](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_dev_mode");

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.static_dir(public_dir, "/assets");

    assert(app.isDevMode() == true);

    app.setDevMode(false);

    assert(app.isDevMode() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_does_not_change_config_unless_it_chooses_to()
  {
    prepare_app_env();

    App::set_static_handler(
        [](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_config_unchanged");

    App app;

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    app.static_dir(public_dir, "/assets");

    assert(app.config().getServerPort() == 18080);
    assert(app.config().getString("app.name", "missing") == "vix");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_can_mutate_app_config()
  {
    prepare_app_env();

    App::set_static_handler(
        [](
            App &app,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          app.config().setServerPort(19090);
          app.config().set("static.handler.enabled", true);
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_config_mutation");

    App app;

    assert(app.config().getServerPort() == 8080);

    app.static_dir(public_dir, "/assets");

    assert(app.config().getServerPort() == 19090);
    assert(app.config().getBool("static.handler.enabled", false) == true);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_can_register_routes_and_static_dir_still_does_not_start_server()
  {
    prepare_app_env();

    App::set_static_handler(
        [](
            App &app,
            const std::filesystem::path &,
            const std::string &mount,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          app.get(
              mount + "/status",
              [](Request &, ResponseWrapper &res)
              {
                res.ok().text("static status");
              });

          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_route_no_server");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(app.is_running() == false);
    assert_route_registered(app, "GET", "/assets/status");
    assert_route_registered(app, "OPTIONS", "/assets/status");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_works_after_templates()
  {
    prepare_app_env();

    int calls = 0;

    App::set_static_handler(
        [&calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;
          return false;
        });

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_hook_after_templates");

    const std::filesystem::path views_dir = root / "views";
    const std::filesystem::path public_dir = root / "public";

    write_file(views_dir / "index.html", "<html>{{ title }}</html>");
    write_file(public_dir / "index.html", "home");

    App app;

    app.templates(views_dir.string());

    assert(app.has_views() == true);

    app.static_dir(public_dir, "/assets");

    assert(calls == 1);
    assert(app.has_views() == true);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_works_before_templates()
  {
    prepare_app_env();

    int calls = 0;

    App::set_static_handler(
        [&calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;
          return false;
        });

    const std::filesystem::path root =
        make_temp_dir("vix_app_static_hook_before_templates");

    const std::filesystem::path views_dir = root / "views";
    const std::filesystem::path public_dir = root / "public";

    write_file(views_dir / "index.html", "<html>{{ title }}</html>");
    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/assets");
    app.templates(views_dir.string());

    assert(calls == 1);
    assert(app.has_views() == true);

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_with_existing_dynamic_routes()
  {
    prepare_app_env();

    int calls = 0;

    App::set_static_handler(
        [&calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_existing_routes");

    App app;

    app.get("/api/status", text_handler);
    app.post("/api/users", text_handler);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "POST", "/api/users");

    app.static_dir(public_dir, "/assets");

    assert(calls == 1);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "POST", "/api/users");

    app.close();

    reset_static_hooks();
  }

  static void test_static_handler_with_group_routes()
  {
    prepare_app_env();

    int calls = 0;

    App::set_static_handler(
        [&calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++calls;
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hook_group_routes");

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
        });

    app.static_dir(public_dir, "/assets");

    assert(calls == 1);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();

    reset_static_hooks();
  }

  static void test_static_response_hook_contract_can_mutate_response()
  {
    prepare_app_env();

    Request req{
        std::string{"GET"},
        std::string{"/assets/app.js"}};

    Response response;
    ResponseWrapper res{response};

    App::StaticResponseHook hook =
        [](const Request &input, ResponseWrapper &output)
    {
      assert(input.method() == "GET");
      assert(input.path() == "/assets/app.js");

      output.header("X-Static-Hook", "yes");
      output.header("Vary", "Accept-Encoding");
    };

    hook(req, res);

    assert(response.header("X-Static-Hook") == "yes");
    assert(response.header("Vary") == "Accept-Encoding");

    reset_static_hooks();
  }

  static void test_static_response_hook_contract_can_inspect_head_request()
  {
    prepare_app_env();

    Request req{
        std::string{"HEAD"},
        std::string{"/assets/app.css"}};

    Response response;
    ResponseWrapper res{response};

    bool saw_head = false;

    App::StaticResponseHook hook =
        [&saw_head](const Request &input, ResponseWrapper &output)
    {
      if (input.method() == "HEAD")
      {
        saw_head = true;
        output.header("X-Static-Method", "HEAD");
      }
    };

    hook(req, res);

    assert(saw_head == true);
    assert(response.header("X-Static-Method") == "HEAD");

    reset_static_hooks();
  }

  static void test_static_response_hook_contract_can_skip_non_get_methods()
  {
    prepare_app_env();

    Request req{
        std::string{"POST"},
        std::string{"/assets/app.js"}};

    Response response;
    ResponseWrapper res{response};

    bool touched = false;

    App::StaticResponseHook hook =
        [&touched](const Request &input, ResponseWrapper &output)
    {
      if (input.method() == "GET" || input.method() == "HEAD")
      {
        touched = true;
        output.header("X-Static-Hook", "yes");
      }
    };

    hook(req, res);

    assert(touched == false);
    assert(response.header("X-Static-Hook") == "");

    reset_static_hooks();
  }

  static void test_static_response_hook_contract_can_preserve_existing_encoding()
  {
    prepare_app_env();

    Request req{
        std::string{"GET"},
        std::string{"/assets/app.js"}};

    Response response;
    ResponseWrapper res{response};

    response.set_header("Content-Encoding", "br");

    App::StaticResponseHook hook =
        [](const Request &, ResponseWrapper &output)
    {
      if (output.res.header("Content-Encoding").empty())
      {
        output.header("Content-Encoding", "gzip");
      }

      output.header("X-Static-Hook", "checked");
    };

    hook(req, res);

    assert(response.header("Content-Encoding") == "br");
    assert(response.header("X-Static-Hook") == "checked");

    reset_static_hooks();
  }

  static void test_static_response_hook_contract_can_add_cache_headers()
  {
    prepare_app_env();

    Request req{
        std::string{"GET"},
        std::string{"/assets/app.js"}};

    Response response;
    ResponseWrapper res{response};

    App::StaticResponseHook hook =
        [](const Request &, ResponseWrapper &output)
    {
      output.header("Cache-Control", "public, max-age=60");
      output.header("X-Static-Optimized", "true");
    };

    hook(req, res);

    assert(response.header("Cache-Control") == "public, max-age=60");
    assert(response.header("X-Static-Optimized") == "true");

    reset_static_hooks();
  }

  static void test_set_static_response_hook_accepts_lambda()
  {
    prepare_app_env();

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "installed");
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_response_hook_install");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/");

    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_set_static_response_hook_can_be_replaced()
  {
    prepare_app_env();

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "first");
        });

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "second");
        });

    Request req{
        std::string{"GET"},
        std::string{"/assets/app.js"}};

    Response response;
    ResponseWrapper res{response};

    /*
     * The installed global hook is private to App. This direct hook object
     * validates the public hook signature used by set_static_response_hook().
     */
    App::StaticResponseHook direct =
        [](const Request &, ResponseWrapper &output)
    {
      output.header("X-Static-Hook", "second");
    };

    direct(req, res);

    assert(response.header("X-Static-Hook") == "second");

    reset_static_hooks();
  }

  static void test_set_static_response_hook_can_be_cleared()
  {
    prepare_app_env();

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "installed");
        });

    App::set_static_response_hook({});

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_response_hook_clear");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/");

    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_response_hook_installation_does_not_start_server()
  {
    prepare_app_env();

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "installed");
        });

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_response_hook_no_server");

    write_file(public_dir / "index.html", "home");

    app.static_dir(public_dir, "/");

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_hooks_can_be_installed_together()
  {
    prepare_app_env();

    int handler_calls = 0;

    App::set_static_handler(
        [&handler_calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++handler_calls;
          return false;
        });

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "yes");
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hooks_together");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(handler_calls == 1);
    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_hooks_can_be_reset_together()
  {
    prepare_app_env();

    int handler_calls = 0;

    App::set_static_handler(
        [&handler_calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++handler_calls;
          return false;
        });

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "yes");
        });

    reset_static_hooks();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hooks_reset");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/assets");

    assert(handler_calls == 0);
    assert(app.is_running() == false);

    app.close();

    reset_static_hooks();
  }

  static void test_static_hooks_survive_close_before_listen()
  {
    prepare_app_env();

    int handler_calls = 0;

    App::set_static_handler(
        [&handler_calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          ++handler_calls;
          return false;
        });

    App::set_static_response_hook(
        [](const Request &, ResponseWrapper &res)
        {
          res.header("X-Static-Hook", "yes");
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_static_hooks_close");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/assets");
    app.get("/api/status", text_handler);

    assert(handler_calls == 1);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    app.close();

    assert(app.is_running() == false);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");

    reset_static_hooks();
  }

} // namespace

int main()
{
  test_static_handler_type_traits();

  test_set_static_handler_accepts_empty_handler();
  test_set_static_response_hook_accepts_empty_hook();

  test_static_handler_is_called_by_static_dir();
  test_static_handler_receives_custom_static_dir_options();

  test_static_handler_normalizes_mount_without_leading_slash();
  test_static_handler_normalizes_mount_with_trailing_slash();
  test_static_handler_normalizes_empty_mount_to_root();
  test_static_handler_normalizes_root_mount_to_root();

  test_static_handler_return_true_consumes_static_dir_registration();
  test_static_handler_return_false_allows_core_static_registration();

  test_static_handler_can_register_custom_route();
  test_static_handler_custom_route_can_normalize_mount_itself();

  test_static_handler_can_be_replaced();
  test_static_handler_can_be_cleared();

  test_static_handler_is_global_across_app_instances();
  test_static_handler_receives_each_static_dir_call();

  test_static_handler_does_not_start_server();
  test_static_handler_does_not_change_dev_mode();
  test_static_handler_does_not_change_config_unless_it_chooses_to();
  test_static_handler_can_mutate_app_config();
  test_static_handler_can_register_routes_and_static_dir_still_does_not_start_server();

  test_static_handler_works_after_templates();
  test_static_handler_works_before_templates();
  test_static_handler_with_existing_dynamic_routes();
  test_static_handler_with_group_routes();

  test_static_response_hook_contract_can_mutate_response();
  test_static_response_hook_contract_can_inspect_head_request();
  test_static_response_hook_contract_can_skip_non_get_methods();
  test_static_response_hook_contract_can_preserve_existing_encoding();
  test_static_response_hook_contract_can_add_cache_headers();

  test_set_static_response_hook_accepts_lambda();
  test_set_static_response_hook_can_be_replaced();
  test_set_static_response_hook_can_be_cleared();
  test_static_response_hook_installation_does_not_start_server();

  test_static_hooks_can_be_installed_together();
  test_static_hooks_can_be_reset_together();
  test_static_hooks_survive_close_before_listen();

  reset_static_hooks();

  return 0;
}
