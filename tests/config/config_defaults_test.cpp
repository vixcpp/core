/**
 *
 * @file config_defaults_test.cpp
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
#include <type_traits>

#include <vix/config/Config.hpp>
#include <vix/server/TlsConfig.hpp>

namespace
{
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

    set_env_var("VIX_ENV_SILENT", "true");
  }

  static std::filesystem::path make_empty_env_path()
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("vix_config_defaults_test_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    assert(!ec);

    return dir / ".env";
  }

  static Config make_default_config()
  {
    clear_config_env();

    const std::filesystem::path env_path = make_empty_env_path();

    Config config{env_path};

    return config;
  }

  static void test_config_type_traits()
  {
    static_assert(std::is_constructible_v<Config>);
    static_assert(std::is_constructible_v<Config, std::filesystem::path>);

    static_assert(std::is_copy_constructible_v<Config>);
    static_assert(std::is_copy_assignable_v<Config>);

    static_assert(std::is_move_constructible_v<Config>);
    static_assert(std::is_move_assignable_v<Config>);

    static_assert(std::is_destructible_v<Config>);
  }

  static void test_default_database_values()
  {
    Config config = make_default_config();

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
    assert(config.getDbPasswordFromEnv() == "");
  }

  static void test_default_server_values()
  {
    Config config = make_default_config();

    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);
  }

  static void test_default_bench_mode()
  {
    Config config = make_default_config();

#if defined(VIX_BENCH_MODE)
    assert(config.isBenchMode() == true);
#else
    assert(config.isBenchMode() == false);
#endif
  }

  static void test_default_logging_values()
  {
    Config config = make_default_config();

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_default_waf_values()
  {
    Config config = make_default_config();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_default_tls_values()
  {
    Config config = make_default_config();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "");
    assert(tls.key_file == "");

    assert(tls.is_enabled() == false);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_missing_dotted_keys_return_false_for_has()
  {
    Config config = make_default_config();

    assert(config.has("") == false);
    assert(config.has("missing") == false);
    assert(config.has("server.port") == false);
    assert(config.has("database.default.host") == false);
    assert(config.has("waf.mode") == false);
    assert(config.has("tls.enabled") == false);
  }

  static void test_get_int_returns_default_for_missing_key()
  {
    Config config = make_default_config();

    assert(config.getInt("", 123) == 123);
    assert(config.getInt("missing", 456) == 456);
    assert(config.getInt("server.port", 7777) == 7777);
    assert(config.getInt("database.default.port", 9999) == 9999);
  }

  static void test_get_bool_returns_default_for_missing_key()
  {
    Config config = make_default_config();

    assert(config.getBool("", true) == true);
    assert(config.getBool("", false) == false);

    assert(config.getBool("missing", true) == true);
    assert(config.getBool("missing", false) == false);

    assert(config.getBool("server.tls.enabled", true) == true);
    assert(config.getBool("server.tls.enabled", false) == false);
  }

  static void test_get_string_returns_default_for_missing_key()
  {
    Config config = make_default_config();

    assert(config.getString("", "fallback") == "fallback");
    assert(config.getString("missing", "fallback") == "fallback");
    assert(config.getString("database.default.host", "db.local") == "db.local");
    assert(config.getString("server.tls.cert_file", "cert.pem") == "cert.pem");
  }

  static void test_set_server_port_changes_server_port_without_env()
  {
    Config config = make_default_config();

    assert(config.getServerPort() == 8080);

    config.setServerPort(9090);

    assert(config.getServerPort() == 9090);
    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 9090);
    assert(config.getString("server.port", "missing") == "9090");
  }

  static void test_load_config_resets_manual_raw_config_values()
  {
    Config config = make_default_config();

    config.set("app.name", "vix");
    config.set("server.port", 9090);

    assert(config.has("app.name") == true);
    assert(config.getString("app.name", "") == "vix");
    assert(config.getInt("server.port", -1) == 9090);

    config.loadConfig();

    assert(config.has("app.name") == false);
    assert(config.getString("app.name", "fallback") == "fallback");

    assert(config.getServerPort() == 8080);
    assert(config.has("server.port") == false);
    assert(config.getInt("server.port", 1234) == 1234);
  }

  static void test_constructing_multiple_default_configs_is_stable()
  {
    Config first = make_default_config();
    Config second = make_default_config();

    assert(first.getServerPort() == 8080);
    assert(second.getServerPort() == 8080);

    assert(first.getDbHost() == "localhost");
    assert(second.getDbHost() == "localhost");

    assert(first.getWafMode() == "basic");
    assert(second.getWafMode() == "basic");

    assert(first.getLogAsync() == true);
    assert(second.getLogAsync() == true);

    assert(first.isTlsEnabled() == false);
    assert(second.isTlsEnabled() == false);
  }

} // namespace

int main()
{
  test_config_type_traits();

  test_default_database_values();
  test_default_server_values();
  test_default_bench_mode();

  test_default_logging_values();
  test_default_waf_values();
  test_default_tls_values();

  test_missing_dotted_keys_return_false_for_has();

  test_get_int_returns_default_for_missing_key();
  test_get_bool_returns_default_for_missing_key();
  test_get_string_returns_default_for_missing_key();

  test_set_server_port_changes_server_port_without_env();
  test_load_config_resets_manual_raw_config_values();

  test_constructing_multiple_default_configs_is_stable();

  return 0;
}
