/**
 *
 * @file app_config_test.cpp
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
#include <string>

#include <vix/app/App.hpp>
#include <vix/config/Config.hpp>
#include <vix/server/TlsConfig.hpp>

namespace
{
  using App = vix::App;
  using Config = vix::config::Config;
  using TlsConfig = vix::server::TlsConfig;

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

  static void clear_config_env()
  {
    unset_env_var("DATABASE_DEFAULT_HOST");
    unset_env_var("DATABASE_DEFAULT_USER");
    unset_env_var("DATABASE_DEFAULT_NAME");
    unset_env_var("DATABASE_DEFAULT_PORT");
    unset_env_var("DATABASE_DEFAULT_PASSWORD");

    unset_env_var("VIX_DB_PASSWORD");
    unset_env_var("DB_PASSWORD");
    unset_env_var("MYSQL_PASSWORD");

    unset_env_var("SERVER_PORT");
    unset_env_var("SERVER_REQUEST_TIMEOUT");
    unset_env_var("SERVER_IO_THREADS");
    unset_env_var("SERVER_SESSION_TIMEOUT_SEC");
    unset_env_var("SERVER_BENCH_MODE");

    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");

    unset_env_var("LOGGING_ASYNC");
    unset_env_var("LOGGING_QUEUE_MAX");
    unset_env_var("LOGGING_DROP_ON_OVERFLOW");

    unset_env_var("WAF_MODE");
    unset_env_var("WAF_MAX_TARGET_LEN");
    unset_env_var("WAF_MAX_BODY_BYTES");

    unset_env_var("APP_NAME");
    unset_env_var("APP_PORT");
    unset_env_var("APP_DEBUG");
    unset_env_var("FEATURE_ENABLED");
    unset_env_var("FEATURE_COUNT");

    set_env_var("VIX_ENV_SILENT", "true");
  }

  static void prepare_app_env()
  {
    clear_config_env();

    set_env_var("VIX_DOCS", "false");
    set_env_var("VIX_ACCESS_LOGS", "false");
    set_env_var("VIX_INTERNAL_LOGS", "false");
    set_env_var("VIX_LOG_ASYNC", "false");
    set_env_var("VIX_LOG_LEVEL", "critical");
  }

  static std::filesystem::path make_empty_env_path()
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("vix_app_config_test_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    assert(!ec);

    return dir / ".env";
  }

  static Config make_clean_config()
  {
    clear_config_env();

    const std::filesystem::path env_path = make_empty_env_path();

    Config config{env_path};

    return config;
  }

  static Config make_config_from_current_env()
  {
    const std::filesystem::path env_path = make_empty_env_path();

    Config config{env_path};

    return config;
  }

  static Config make_populated_config()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18180");
    set_env_var("SERVER_REQUEST_TIMEOUT", "4500");
    set_env_var("SERVER_IO_THREADS", "4");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "45");
    set_env_var("SERVER_BENCH_MODE", "true");

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "2048");
    set_env_var("WAF_MAX_BODY_BYTES", "65536");

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "4096");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    config.set("app.name", "Vix.cpp");
    config.set("app.port", 3000);
    config.set("app.debug", true);
    config.set("feature.enabled", true);
    config.set("feature.count", 7);

    return config;
  }

  static void assert_default_app_config(const Config &config)
  {
    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);

#if defined(VIX_BENCH_MODE)
    assert(config.isBenchMode() == true);
#else
    assert(config.isBenchMode() == false);
#endif

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");
  }

  static void assert_populated_config(const Config &config)
  {
    assert(config.getServerPort() == 18180);
    assert(config.getRequestTimeout() == 4500);
    assert(config.getIOThreads() == 4);
    assert(config.getSessionTimeoutSec() == 45);
    assert(config.isBenchMode() == true);

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 2048);
    assert(config.getWafMaxBodyBytes() == 65536);

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 4096);
    assert(config.getLogDropOnOverflow() == false);

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "key.pem");
    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == true);

    assert(config.has("app.name") == true);
    assert(config.has("app.port") == true);
    assert(config.has("app.debug") == true);
    assert(config.has("feature.enabled") == true);
    assert(config.has("feature.count") == true);

    assert(config.getString("app.name", "missing") == "Vix.cpp");
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getBool("app.debug", false) == true);
    assert(config.getBool("feature.enabled", false) == true);
    assert(config.getInt("feature.count", -1) == 7);
  }

  static void test_app_config_defaults()
  {
    prepare_app_env();

    App app;

    assert_default_app_config(app.config());

    app.close();
  }

  static void test_app_config_reference_is_mutable()
  {
    prepare_app_env();

    App app;

    Config &config = app.config();

    assert(config.getServerPort() == 8080);

    config.setServerPort(18080);
    config.set("app.name", "vix");
    config.set("app.debug", true);

    assert(app.config().getServerPort() == 18080);
    assert(app.config().has("server.port") == true);
    assert(app.config().getInt("server.port", -1) == 18080);

    assert(app.config().has("app.name") == true);
    assert(app.config().getString("app.name", "missing") == "vix");

    assert(app.config().has("app.debug") == true);
    assert(app.config().getBool("app.debug", false) == true);

    app.close();
  }

  static void test_app_config_reference_is_stable()
  {
    prepare_app_env();

    App app;

    Config *first = &app.config();
    Config *second = &app.config();

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first == second);

    app.close();
  }

  static void test_app_config_set_server_port_updates_raw_config()
  {
    prepare_app_env();

    App app;

    app.config().setServerPort(19090);

    assert(app.config().getServerPort() == 19090);
    assert(app.config().has("server.port") == true);
    assert(app.config().getInt("server.port", -1) == 19090);
    assert(app.config().getString("server.port", "missing") == "19090");
    assert(app.config().getBool("server.port", false) == true);

    app.close();
  }

  static void test_app_config_set_and_get_custom_values()
  {
    prepare_app_env();

    App app;

    app.config().set("app.name", "Vix.cpp");
    app.config().set("app.port", 3000);
    app.config().set("app.debug", true);
    app.config().set("feature.enabled", false);
    app.config().set("feature.count", 7);

    assert(app.config().has("app") == true);
    assert(app.config().has("app.name") == true);
    assert(app.config().has("app.port") == true);
    assert(app.config().has("app.debug") == true);
    assert(app.config().has("feature.enabled") == true);
    assert(app.config().has("feature.count") == true);

    assert(app.config().getString("app.name", "missing") == "Vix.cpp");
    assert(app.config().getInt("app.port", -1) == 3000);
    assert(app.config().getBool("app.debug", false) == true);
    assert(app.config().getBool("feature.enabled", true) == false);
    assert(app.config().getInt("feature.count", -1) == 7);

    app.close();
  }

  static void test_app_config_can_be_replaced_by_assignment()
  {
    Config cfg = make_populated_config();

    prepare_app_env();

    App app;

    assert_default_app_config(app.config());

    app.config() = cfg;

    assert_populated_config(app.config());

    app.close();
  }

  static void test_app_config_assignment_keeps_independent_copy()
  {
    Config cfg = make_populated_config();

    prepare_app_env();

    App app;

    app.config() = cfg;

    assert_populated_config(app.config());

    cfg.setServerPort(20000);
    cfg.set("app.name", "changed");
    cfg.set("feature.count", 99);

    assert(cfg.getServerPort() == 20000);
    assert(cfg.getString("app.name", "missing") == "changed");
    assert(cfg.getInt("feature.count", -1) == 99);

    assert(app.config().getServerPort() == 18180);
    assert(app.config().getString("app.name", "missing") == "Vix.cpp");
    assert(app.config().getInt("feature.count", -1) == 7);

    app.close();
  }

  static void test_app_config_can_be_reset_to_clean_config()
  {
    Config populated = make_populated_config();

    prepare_app_env();

    App app;

    app.config() = populated;

    assert_populated_config(app.config());

    Config clean = make_clean_config();

    app.config() = clean;

    assert_default_app_config(app.config());

    assert(app.config().has("app.name") == false);
    assert(app.config().has("app.port") == false);
    assert(app.config().has("app.debug") == false);
    assert(app.config().has("feature.enabled") == false);
    assert(app.config().has("feature.count") == false);

    app.close();
  }

  static void test_app_config_load_config_resets_manual_raw_values()
  {
    prepare_app_env();

    App app;

    app.config().set("app.name", "vix");
    app.config().set("app.port", 3000);
    app.config().set("app.debug", true);
    app.config().setServerPort(18080);

    assert(app.config().has("app.name") == true);
    assert(app.config().has("app.port") == true);
    assert(app.config().has("app.debug") == true);
    assert(app.config().has("server.port") == true);

    assert(app.config().getServerPort() == 18080);

    app.config().loadConfig();

    assert(app.config().has("app.name") == false);
    assert(app.config().has("app.port") == false);
    assert(app.config().has("app.debug") == false);
    assert(app.config().has("server.port") == false);

    assert(app.config().getString("app.name", "fallback") == "fallback");
    assert(app.config().getInt("app.port", 123) == 123);
    assert(app.config().getBool("app.debug", false) == false);

    assert(app.config().getServerPort() == 8080);

    app.close();
  }

  static void test_app_config_reads_environment_on_app_construction()
  {
    prepare_app_env();

    set_env_var("SERVER_PORT", "18280");
    set_env_var("SERVER_REQUEST_TIMEOUT", "3333");
    set_env_var("SERVER_IO_THREADS", "2");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "55");

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "1111");
    set_env_var("WAF_MAX_BODY_BYTES", "2222");

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "3333");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "env-cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "env-key.pem");

    App app;

    assert(app.config().getServerPort() == 18280);
    assert(app.config().getRequestTimeout() == 3333);
    assert(app.config().getIOThreads() == 2);
    assert(app.config().getSessionTimeoutSec() == 55);

    assert(app.config().getWafMode() == "strict");
    assert(app.config().getWafMaxTargetLen() == 1111);
    assert(app.config().getWafMaxBodyBytes() == 2222);

    assert(app.config().getLogAsync() == false);
    assert(app.config().getLogQueueMax() == 3333);
    assert(app.config().getLogDropOnOverflow() == false);

    assert(app.config().isTlsEnabled() == true);
    assert(app.config().getTlsCertFile() == "env-cert.pem");
    assert(app.config().getTlsKeyFile() == "env-key.pem");

    const TlsConfig tls = app.config().getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "env-cert.pem");
    assert(tls.key_file == "env-key.pem");
    assert(tls.is_valid() == true);

    app.close();
  }

  static void test_app_config_reload_reads_updated_environment()
  {
    prepare_app_env();

    set_env_var("SERVER_PORT", "18380");
    set_env_var("WAF_MODE", "basic");
    set_env_var("LOGGING_QUEUE_MAX", "1000");

    App app;

    assert(app.config().getServerPort() == 18380);
    assert(app.config().getWafMode() == "basic");
    assert(app.config().getLogQueueMax() == 1000);

    set_env_var("SERVER_PORT", "18381");
    set_env_var("WAF_MODE", "strict");
    set_env_var("LOGGING_QUEUE_MAX", "2000");

    app.config().loadConfig();

    assert(app.config().getServerPort() == 18381);
    assert(app.config().getWafMode() == "strict");
    assert(app.config().getLogQueueMax() == 2000);

    app.close();
  }

  static void test_app_config_reload_resets_to_defaults_when_env_removed()
  {
    prepare_app_env();

    set_env_var("SERVER_PORT", "18480");
    set_env_var("WAF_MODE", "strict");
    set_env_var("LOGGING_QUEUE_MAX", "3000");
    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    App app;

    assert(app.config().getServerPort() == 18480);
    assert(app.config().getWafMode() == "strict");
    assert(app.config().getLogQueueMax() == 3000);
    assert(app.config().isTlsEnabled() == true);

    unset_env_var("SERVER_PORT");
    unset_env_var("WAF_MODE");
    unset_env_var("LOGGING_QUEUE_MAX");
    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");

    app.config().loadConfig();

    assert(app.config().getServerPort() == 8080);
    assert(app.config().getWafMode() == "basic");
    assert(app.config().getLogQueueMax() == 20000);
    assert(app.config().isTlsEnabled() == false);
    assert(app.config().getTlsCertFile() == "");
    assert(app.config().getTlsKeyFile() == "");

    app.close();
  }

  static void test_app_config_does_not_start_server()
  {
    prepare_app_env();

    App app;

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.config().setServerPort(18080);
    app.config().set("app.name", "vix");

    assert(app.is_running() == false);
    assert(app.has_server_ready_info() == false);

    app.close();

    assert(app.is_running() == false);
  }

} // namespace

int main()
{
  test_app_config_defaults();

  test_app_config_reference_is_mutable();
  test_app_config_reference_is_stable();

  test_app_config_set_server_port_updates_raw_config();
  test_app_config_set_and_get_custom_values();

  test_app_config_can_be_replaced_by_assignment();
  test_app_config_assignment_keeps_independent_copy();
  test_app_config_can_be_reset_to_clean_config();

  test_app_config_load_config_resets_manual_raw_values();

  test_app_config_reads_environment_on_app_construction();
  test_app_config_reload_reads_updated_environment();
  test_app_config_reload_resets_to_defaults_when_env_removed();

  test_app_config_does_not_start_server();

  return 0;
}
