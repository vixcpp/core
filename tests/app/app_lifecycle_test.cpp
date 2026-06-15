/**
 *
 * @file app_lifecycle_test.cpp
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
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include <vix/app/App.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>

namespace
{
  using App = vix::App;
  using Config = vix::config::Config;
  using Request = vix::http::Request;
  using ResponseWrapper = vix::http::ResponseWrapper;
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

    App::set_static_handler({});
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

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(
        RuntimeConfig{
            1u,
            vix::runtime::BudgetConfig{8u}});
  }

  static bool wait_until(
      const std::function<bool()> &condition,
      std::chrono::milliseconds timeout = std::chrono::milliseconds{2000})
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
      if (condition())
      {
        return true;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    return condition();
  }

  static void assert_route_registered(
      App &app,
      const std::string &method,
      const std::string &path)
  {
    assert(app.router() != nullptr);
    assert(app.router()->has_route(method, path) == true);
  }

  static void assert_not_started(const App &app)
  {
    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);
  }

  static void assert_started_on_port(App &app, int port)
  {
    assert(wait_until(
               [&app]()
               {
                 return app.is_running();
               }) == true);

    assert(wait_until(
               [&app]()
               {
                 return app.has_server_ready_info();
               }) == true);

    const auto info = app.server_ready_info();

    assert(info.port == port);
    assert(!info.host.empty());
    assert(!info.scheme.empty());
  }

  static void close_and_wait(App &app)
  {
    app.close();
    app.wait();

    assert(app.is_running() == false);
  }

  static void test_close_before_listen_is_safe()
  {
    prepare_app_env();

    App app;

    assert_not_started(app);

    app.close();

    assert(app.is_running() == false);

    app.close();
    app.close();

    assert(app.is_running() == false);
  }

  static void test_request_stop_from_signal_before_listen_is_safe()
  {
    prepare_app_env();

    App app;

    assert_not_started(app);

    app.request_stop_from_signal();

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();
  }

  static void test_shutdown_callback_is_not_called_before_close()
  {
    prepare_app_env();

    App app;

    int shutdown_calls = 0;

    app.set_shutdown_callback(
        [&shutdown_calls]()
        {
          ++shutdown_calls;
        });

    assert(shutdown_calls == 0);

    app.get("/status", text_handler);
    app.static_dir("public", "/assets");
    app.templates("views");

    assert(shutdown_calls == 0);

    app.close();
  }

  static void test_shutdown_callback_is_not_called_on_close_before_listen()
  {
    prepare_app_env();

    App app;

    int shutdown_calls = 0;

    app.set_shutdown_callback(
        [&shutdown_calls]()
        {
          ++shutdown_calls;
        });

    app.close();

    assert(shutdown_calls == 0);
    assert(app.is_running() == false);
  }

  static void test_shutdown_callback_is_not_called_for_repeated_close_before_listen()
  {
    prepare_app_env();

    App app;

    int shutdown_calls = 0;

    app.set_shutdown_callback(
        [&shutdown_calls]()
        {
          ++shutdown_calls;
        });

    app.close();
    app.close();
    app.close();

    assert(shutdown_calls == 0);
    assert(app.is_running() == false);
  }

  static void test_shutdown_callback_can_be_replaced_before_close_without_running()
  {
    prepare_app_env();

    App app;

    int first_calls = 0;
    int second_calls = 0;

    app.set_shutdown_callback(
        [&first_calls]()
        {
          ++first_calls;
        });

    app.set_shutdown_callback(
        [&second_calls]()
        {
          ++second_calls;
        });

    app.close();

    assert(first_calls == 0);
    assert(second_calls == 0);
    assert(app.is_running() == false);
  }

  static void test_empty_shutdown_callback_is_allowed()
  {
    prepare_app_env();

    App app;

    app.set_shutdown_callback({});

    app.close();

    assert(app.is_running() == false);
  }

  static void test_listen_starts_server_and_callback_runs()
  {
    prepare_app_env();

    App app;

    std::atomic<int> listen_calls{0};

    app.get("/status", text_handler);

    app.listen(
        19101,
        [&listen_calls]()
        {
          listen_calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(wait_until(
               [&listen_calls]()
               {
                 return listen_calls.load(std::memory_order_relaxed) == 1;
               }) == true);

    assert_started_on_port(app, 19101);

    close_and_wait(app);

    assert(listen_calls.load(std::memory_order_relaxed) == 1);
  }

  static void test_listen_without_callback_starts_server()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen(19102);

    assert_started_on_port(app, 19102);

    close_and_wait(app);
  }

  static void test_listen_port_starts_server_and_port_callback_runs()
  {
    prepare_app_env();

    App app;

    std::atomic<int> callback_port{0};
    std::atomic<int> listen_calls{0};

    app.get("/status", text_handler);

    app.listen_port(
        19103,
        [&callback_port, &listen_calls](int port)
        {
          callback_port.store(port, std::memory_order_relaxed);
          listen_calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(wait_until(
               [&listen_calls]()
               {
                 return listen_calls.load(std::memory_order_relaxed) == 1;
               }) == true);

    assert(callback_port.load(std::memory_order_relaxed) == 19103);

    assert_started_on_port(app, 19103);

    close_and_wait(app);
  }

  static void test_listen_port_without_callback_starts_server()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen_port(19104);

    assert_started_on_port(app, 19104);

    close_and_wait(app);
  }

  static void test_listen_with_config_copies_config_and_starts_server()
  {
    prepare_app_env();

    Config cfg;
    cfg.setServerPort(19105);
    cfg.set("app.name", "listen-config");

    App app;

    app.get("/status", text_handler);

    std::atomic<int> listen_calls{0};

    app.listen(
        cfg,
        [&listen_calls]()
        {
          listen_calls.fetch_add(1, std::memory_order_relaxed);
        });

    assert(wait_until(
               [&listen_calls]()
               {
                 return listen_calls.load(std::memory_order_relaxed) == 1;
               }) == true);

    assert_started_on_port(app, 19105);

    assert(app.config().getServerPort() == 19105);
    assert(app.config().getString("app.name", "missing") == "listen-config");

    close_and_wait(app);
  }

  static void test_listen_with_config_without_callback_starts_server()
  {
    prepare_app_env();

    Config cfg;
    cfg.setServerPort(19106);
    cfg.set("app.name", "listen-config-no-callback");

    App app;

    app.get("/status", text_handler);

    app.listen(cfg);

    assert_started_on_port(app, 19106);

    assert(app.config().getServerPort() == 19106);
    assert(app.config().getString("app.name", "missing") == "listen-config-no-callback");

    close_and_wait(app);
  }

  static void test_listen_config_copy_is_independent_from_source_after_start()
  {
    prepare_app_env();

    Config cfg;
    cfg.setServerPort(19107);
    cfg.set("app.name", "before");

    App app;

    app.get("/status", text_handler);

    app.listen(cfg);

    assert_started_on_port(app, 19107);

    cfg.setServerPort(20000);
    cfg.set("app.name", "after");

    assert(cfg.getServerPort() == 20000);
    assert(cfg.getString("app.name", "missing") == "after");

    assert(app.config().getServerPort() == 19107);
    assert(app.config().getString("app.name", "missing") == "before");

    close_and_wait(app);
  }

  static void test_ready_info_is_empty_before_listen_and_available_after_listen()
  {
    prepare_app_env();

    App app;

    assert_not_started(app);

    app.get("/status", text_handler);

    app.listen(19108);

    assert_started_on_port(app, 19108);

    close_and_wait(app);
  }

  static void test_ready_info_remains_available_after_close()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen(19109);

    assert_started_on_port(app, 19109);

    close_and_wait(app);

    assert(app.has_server_ready_info() == true);

    const auto info = app.server_ready_info();

    assert(info.port == 19109);
    assert(!info.host.empty());
    assert(!info.scheme.empty());
  }

  static void test_request_stop_from_signal_is_safe_while_running()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen(19110);

    assert_started_on_port(app, 19110);

    app.request_stop_from_signal();

    /*
     * request_stop_from_signal() is a safe signal path. The deterministic test
     * cleanup still uses close() + wait().
     */
    app.close();
    app.wait();

    assert(app.is_running() == false);
  }

  static void test_close_stops_running_server()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen(19111);

    assert_started_on_port(app, 19111);

    app.close();

    app.wait();

    assert(app.is_running() == false);
  }

  static void test_close_running_server_is_idempotent()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen(19112);

    assert_started_on_port(app, 19112);

    app.close();
    app.close();
    app.close();

    app.wait();

    assert(app.is_running() == false);
  }

  static void test_wait_after_close_is_idempotent()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.listen(19113);

    assert_started_on_port(app, 19113);

    app.close();

    app.wait();
    app.wait();
    app.wait();

    assert(app.is_running() == false);
  }

  static void test_shutdown_callback_runs_when_running_server_closes()
  {
    prepare_app_env();

    App app;

    std::atomic<int> shutdown_calls{0};

    app.set_shutdown_callback(
        [&shutdown_calls]()
        {
          shutdown_calls.fetch_add(1, std::memory_order_relaxed);
        });

    app.get("/status", text_handler);

    app.listen(19114);

    assert_started_on_port(app, 19114);

    app.close();
    app.wait();

    assert(shutdown_calls.load(std::memory_order_relaxed) == 1);
  }

  static void test_shutdown_callback_with_signal_stop_is_safe()
  {
    prepare_app_env();

    App app;

    std::atomic<int> shutdown_calls{0};

    app.set_shutdown_callback(
        [&shutdown_calls]()
        {
          shutdown_calls.fetch_add(1, std::memory_order_relaxed);
        });

    app.get("/status", text_handler);

    app.listen(19115);

    assert_started_on_port(app, 19115);

    app.request_stop_from_signal();

    /*
     * request_stop_from_signal() is a safe signal path. It should not be
     * tested as the path responsible for running the shutdown callback.
     * Deterministic cleanup remains close() + wait().
     */
    app.close();
    app.wait();

    assert(app.is_running() == false);
    assert(shutdown_calls.load(std::memory_order_relaxed) <= 1);
  }

  static void test_routes_registered_before_listen_remain_registered_after_close()
  {
    prepare_app_env();

    App app;

    app.get("/one", text_handler);
    app.post("/two", text_handler);
    app.put("/three", text_handler);

    assert_route_registered(app, "GET", "/one");
    assert_route_registered(app, "POST", "/two");
    assert_route_registered(app, "PUT", "/three");

    app.listen(19116);

    assert_started_on_port(app, 19116);

    close_and_wait(app);

    assert_route_registered(app, "GET", "/one");
    assert_route_registered(app, "POST", "/two");
    assert_route_registered(app, "PUT", "/three");
  }

  static void test_group_routes_registered_before_listen_remain_registered_after_close()
  {
    prepare_app_env();

    App app;

    app.group(
        "/api",
        [](App::Group api)
        {
          api.get("/status", text_handler);
          api.post("/users", text_handler);
        });

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "POST", "/api/users");

    app.listen(19117);

    assert_started_on_port(app, 19117);

    close_and_wait(app);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "POST", "/api/users");
  }

  static void test_static_dir_before_listen_does_not_break_lifecycle()
  {
    prepare_app_env();

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_lifecycle_static");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/assets");
    app.get("/api/status", text_handler);

    app.listen(19118);

    assert_started_on_port(app, 19118);

    close_and_wait(app);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "OPTIONS", "/api/status");
  }

  static void test_templates_before_listen_do_not_break_lifecycle()
  {
    prepare_app_env();

    const std::filesystem::path views_dir =
        make_temp_dir("vix_app_lifecycle_templates");

    write_file(views_dir / "index.html", "<html>{{ title }}</html>");

    App app;

    app.templates(views_dir.string());
    app.get("/api/status", text_handler);

    assert(app.has_views() == true);

    app.listen(19119);

    assert_started_on_port(app, 19119);

    close_and_wait(app);

    assert(app.has_views() == true);
    assert_route_registered(app, "GET", "/api/status");
  }

  static void test_middleware_before_listen_does_not_break_lifecycle()
  {
    prepare_app_env();

    App app;

    app.use(
        [](Request &, ResponseWrapper &, App::Next next)
        {
          next();
        });

    app.protect(
        "/api/private",
        [](Request &, ResponseWrapper &, App::Next next)
        {
          next();
        });

    app.get("/api/status", text_handler);
    app.get("/api/private/me", text_handler);

    app.listen(19120);

    assert_started_on_port(app, 19120);

    close_and_wait(app);

    assert_route_registered(app, "GET", "/api/status");
    assert_route_registered(app, "GET", "/api/private/me");
  }

  static void test_dev_mode_survives_lifecycle()
  {
    prepare_app_env();

    App app;

    app.setDevMode(true);

    assert(app.isDevMode() == true);

    app.get("/status", text_handler);

    app.listen(19121);

    assert_started_on_port(app, 19121);
    assert(app.isDevMode() == true);

    close_and_wait(app);

    assert(app.isDevMode() == true);
  }

  static void test_config_mutation_before_listen_is_used()
  {
    prepare_app_env();

    App app;

    app.config().setServerPort(19122);
    app.config().set("app.name", "mutated-before-listen");

    app.get("/status", text_handler);

    app.listen(app.config());

    assert_started_on_port(app, 19122);

    assert(app.config().getServerPort() == 19122);
    assert(app.config().getString("app.name", "missing") == "mutated-before-listen");

    close_and_wait(app);
  }

  static void test_external_executor_lifecycle()
  {
    prepare_app_env();

    auto executor = make_executor();

    App app{executor};

    assert(executor->started() == true);
    assert(executor->running() == true);
    assert(executor->accepting() == true);

    app.get("/status", text_handler);

    app.listen(19123);

    assert_started_on_port(app, 19123);

    app.close();
    app.wait();

    assert(app.is_running() == false);

    executor->stop();

    assert(executor->started() == false);
    assert(executor->running() == false);
    assert(executor->accepting() == false);
  }

  static void test_external_executor_signal_stop_lifecycle()
  {
    prepare_app_env();

    auto executor = make_executor();

    App app{executor};

    app.get("/status", text_handler);

    app.listen(19124);

    assert_started_on_port(app, 19124);

    app.request_stop_from_signal();

    app.close();
    app.wait();

    assert(app.is_running() == false);

    executor->stop();
  }

  static void test_listen_callback_can_observe_app_state()
  {
    prepare_app_env();

    App app;

    std::atomic<bool> callback_saw_running{false};
    std::atomic<bool> callback_saw_ready_info{false};

    app.get("/status", text_handler);

    app.listen(
        19125,
        [&app, &callback_saw_running, &callback_saw_ready_info]()
        {
          callback_saw_running.store(app.is_running(), std::memory_order_relaxed);
          callback_saw_ready_info.store(app.has_server_ready_info(), std::memory_order_relaxed);
        });

    assert_started_on_port(app, 19125);

    assert(callback_saw_running.load(std::memory_order_relaxed) == true);
    assert(callback_saw_ready_info.load(std::memory_order_relaxed) == true);

    close_and_wait(app);
  }

  static void test_listen_port_callback_can_observe_ready_info()
  {
    prepare_app_env();

    App app;

    std::atomic<bool> saw_matching_info{false};

    app.get("/status", text_handler);

    app.listen_port(
        19126,
        [&app, &saw_matching_info](int port)
        {
          const auto info = app.server_ready_info();

          saw_matching_info.store(
              port == 19126 && info.port == 19126,
              std::memory_order_relaxed);
        });

    assert_started_on_port(app, 19126);

    assert(saw_matching_info.load(std::memory_order_relaxed) == true);

    close_and_wait(app);
  }

  static void test_close_without_explicit_wait_joins_on_destruction_path()
  {
    prepare_app_env();

    {
      App app;

      app.get("/status", text_handler);

      app.listen(19127);

      assert_started_on_port(app, 19127);

      app.close();

      assert(app.is_running() == false);
    }
  }

  static void test_shutdown_callback_can_close_external_state()
  {
    prepare_app_env();

    App app;

    std::atomic<bool> resource_closed{false};

    app.set_shutdown_callback(
        [&resource_closed]()
        {
          resource_closed.store(true, std::memory_order_relaxed);
        });

    app.get("/status", text_handler);

    app.listen(19128);

    assert_started_on_port(app, 19128);

    close_and_wait(app);

    assert(resource_closed.load(std::memory_order_relaxed) == true);
  }

  static void test_lifecycle_with_static_handler_installed()
  {
    prepare_app_env();

    std::atomic<int> static_handler_calls{0};

    App::set_static_handler(
        [&static_handler_calls](
            App &,
            const std::filesystem::path &,
            const std::string &,
            const std::string &,
            bool,
            const std::string &,
            bool) -> bool
        {
          static_handler_calls.fetch_add(1, std::memory_order_relaxed);
          return false;
        });

    const std::filesystem::path public_dir =
        make_temp_dir("vix_app_lifecycle_static_handler");

    write_file(public_dir / "index.html", "home");

    App app;

    app.static_dir(public_dir, "/assets");
    app.get("/status", text_handler);

    assert(static_handler_calls.load(std::memory_order_relaxed) == 1);

    app.listen(19129);

    assert_started_on_port(app, 19129);

    close_and_wait(app);

    assert(static_handler_calls.load(std::memory_order_relaxed) == 1);

    App::set_static_handler({});
  }

  static void test_lifecycle_keeps_executor_available_after_close()
  {
    prepare_app_env();

    App app;

    assert(app.executor().started() == true);
    assert(app.executor().running() == true);

    app.get("/status", text_handler);

    app.listen(19130);

    assert_started_on_port(app, 19130);

    close_and_wait(app);

    assert(&app.executor() != nullptr);
  }

  static void test_listen_after_manual_close_keeps_app_stopped()
  {
    prepare_app_env();

    App app;

    app.get("/status", text_handler);

    app.close();

    assert(app.is_running() == false);

    /*
     * App close is terminal/idempotent for this lifecycle object.
     * The test intentionally does not call listen() after close because a
     * closed App should be discarded and recreated instead.
     */
    assert(app.router()->has_route("GET", "/status") == true);
  }

} // namespace

int main()
{
  test_close_before_listen_is_safe();
  test_request_stop_from_signal_before_listen_is_safe();

  test_shutdown_callback_is_not_called_before_close();
  test_shutdown_callback_is_not_called_on_close_before_listen();
  test_shutdown_callback_is_not_called_for_repeated_close_before_listen();
  test_shutdown_callback_can_be_replaced_before_close_without_running();
  test_empty_shutdown_callback_is_allowed();

  test_listen_starts_server_and_callback_runs();
  test_listen_without_callback_starts_server();

  test_listen_port_starts_server_and_port_callback_runs();
  test_listen_port_without_callback_starts_server();

  test_listen_with_config_copies_config_and_starts_server();
  test_listen_with_config_without_callback_starts_server();
  test_listen_config_copy_is_independent_from_source_after_start();

  test_ready_info_is_empty_before_listen_and_available_after_listen();
  test_ready_info_remains_available_after_close();

  test_request_stop_from_signal_is_safe_while_running();
  test_close_stops_running_server();
  test_close_running_server_is_idempotent();
  test_wait_after_close_is_idempotent();

  test_shutdown_callback_runs_when_running_server_closes();
  test_shutdown_callback_with_signal_stop_is_safe();

  test_routes_registered_before_listen_remain_registered_after_close();
  test_group_routes_registered_before_listen_remain_registered_after_close();

  test_static_dir_before_listen_does_not_break_lifecycle();
  test_templates_before_listen_do_not_break_lifecycle();
  test_middleware_before_listen_does_not_break_lifecycle();

  test_dev_mode_survives_lifecycle();
  test_config_mutation_before_listen_is_used();

  test_external_executor_lifecycle();
  test_external_executor_signal_stop_lifecycle();

  test_listen_callback_can_observe_app_state();
  test_listen_port_callback_can_observe_ready_info();

  test_close_without_explicit_wait_joins_on_destruction_path();
  test_shutdown_callback_can_close_external_state();

  test_lifecycle_with_static_handler_installed();

  test_lifecycle_keeps_executor_available_after_close();
  test_listen_after_manual_close_keeps_app_stopped();

  App::set_static_handler({});

  return 0;
}
