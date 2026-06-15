/**
 *
 * @file config_env_test.cpp
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

    unset_env_var("APP_NAME");
    unset_env_var("APP_DEBUG");
    unset_env_var("APP_PORT");
    unset_env_var("FEATURE_ENABLED");
    unset_env_var("FEATURE_COUNT");

    set_env_var("VIX_ENV_SILENT", "true");
  }

  static std::filesystem::path make_empty_env_path()
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("vix_config_env_test_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    assert(!ec);

    return dir / ".env";
  }

  static Config make_config_from_current_env()
  {
    const std::filesystem::path env_path = make_empty_env_path();

    Config config{env_path};

    return config;
  }

  static void test_server_env_values_override_defaults()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");
    set_env_var("SERVER_REQUEST_TIMEOUT", "4500");
    set_env_var("SERVER_IO_THREADS", "4");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "45");
    set_env_var("SERVER_BENCH_MODE", "true");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 18080);
    assert(config.getRequestTimeout() == 4500);
    assert(config.getIOThreads() == 4);
    assert(config.getSessionTimeoutSec() == 45);

#if defined(VIX_BENCH_MODE)
    assert(config.isBenchMode() == true);
#else
    assert(config.isBenchMode() == true);
#endif
  }

  static void test_server_env_invalid_int_values_fall_back_to_defaults()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "not-a-number");
    set_env_var("SERVER_REQUEST_TIMEOUT", "bad-timeout");
    set_env_var("SERVER_IO_THREADS", "bad-threads");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "bad-session");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);
  }

  static void test_server_env_zero_and_negative_int_values_are_preserved()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "0");
    set_env_var("SERVER_REQUEST_TIMEOUT", "-1");
    set_env_var("SERVER_IO_THREADS", "-2");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "0");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 0);
    assert(config.getRequestTimeout() == -1);
    assert(config.getIOThreads() == -2);
    assert(config.getSessionTimeoutSec() == 0);
  }

  static void test_database_env_values_override_defaults()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "db.internal");
    set_env_var("DATABASE_DEFAULT_USER", "vix_user");
    set_env_var("DATABASE_DEFAULT_NAME", "vix_db");
    set_env_var("DATABASE_DEFAULT_PORT", "5432");
    set_env_var("DATABASE_DEFAULT_PASSWORD", "secret");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "db.internal");
    assert(config.getDbUser() == "vix_user");
    assert(config.getDbName() == "vix_db");
    assert(config.getDbPort() == 5432);
    assert(config.getDbPasswordFromEnv() == "secret");
  }

  static void test_database_env_invalid_port_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "postgres");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == 3306);
  }

  static void test_logging_env_values_override_defaults()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "999");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 999);
    assert(config.getLogDropOnOverflow() == false);
  }

  static void test_logging_env_invalid_values_fall_back_to_defaults()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "maybe");
    set_env_var("LOGGING_QUEUE_MAX", "big");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "maybe");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_waf_env_values_override_defaults()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "2048");
    set_env_var("WAF_MAX_BODY_BYTES", "65536");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 2048);
    assert(config.getWafMaxBodyBytes() == 65536);
  }

  static void test_waf_env_invalid_int_values_fall_back_to_defaults()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "long");
    set_env_var("WAF_MAX_BODY_BYTES", "huge");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_tls_env_values_override_defaults()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "/tmp/fullchain.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "/tmp/privkey.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "/tmp/fullchain.pem");
    assert(config.getTlsKeyFile() == "/tmp/privkey.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "/tmp/fullchain.pem");
    assert(tls.key_file == "/tmp/privkey.pem");

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == true);
  }

  static void test_tls_enabled_without_files_is_not_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "");
    assert(tls.key_file == "");

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_has_reads_environment_using_dotted_key_conversion()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18081");
    set_env_var("APP_NAME", "vix");
    set_env_var("APP_DEBUG", "true");
    set_env_var("FEATURE_ENABLED", "false");

    Config config = make_config_from_current_env();

    assert(config.has("server.port") == true);
    assert(config.has("app.name") == true);
    assert(config.has("app.debug") == true);
    assert(config.has("feature.enabled") == true);

    assert(config.has("server.missing") == false);
    assert(config.has("missing") == false);
    assert(config.has("") == false);
  }

  static void test_get_int_reads_environment_using_dotted_key_conversion()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18082");
    set_env_var("APP_PORT", "3000");
    set_env_var("FEATURE_COUNT", "42");

    Config config = make_config_from_current_env();

    assert(config.getInt("server.port", -1) == 18082);
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getInt("feature.count", -1) == 42);

    assert(config.getInt("missing.value", 123) == 123);
  }

  static void test_get_int_environment_invalid_value_returns_default()
  {
    clear_config_env();

    set_env_var("APP_PORT", "not-a-port");

    Config config = make_config_from_current_env();

    assert(config.has("app.port") == true);
    assert(config.getInt("app.port", 3000) == 3000);
  }

  static void test_get_bool_reads_environment_using_dotted_key_conversion()
  {
    clear_config_env();

    set_env_var("APP_DEBUG", "true");
    set_env_var("FEATURE_ENABLED", "false");

    Config config = make_config_from_current_env();

    assert(config.getBool("app.debug", false) == true);
    assert(config.getBool("feature.enabled", true) == false);

    assert(config.getBool("missing.flag", true) == true);
    assert(config.getBool("missing.flag", false) == false);
  }

  static void test_get_bool_environment_common_values()
  {
    clear_config_env();

    set_env_var("APP_DEBUG", "1");
    set_env_var("FEATURE_ENABLED", "0");

    Config config = make_config_from_current_env();

    assert(config.getBool("app.debug", false) == true);
    assert(config.getBool("feature.enabled", true) == false);

    clear_config_env();

    set_env_var("APP_DEBUG", "yes");
    set_env_var("FEATURE_ENABLED", "no");

    Config second = make_config_from_current_env();

    assert(second.getBool("app.debug", false) == true);
    assert(second.getBool("feature.enabled", true) == false);
  }

  static void test_get_bool_environment_invalid_value_returns_default()
  {
    clear_config_env();

    set_env_var("APP_DEBUG", "maybe");

    Config config = make_config_from_current_env();

    assert(config.has("app.debug") == true);
    assert(config.getBool("app.debug", true) == true);
    assert(config.getBool("app.debug", false) == false);
  }

  static void test_get_string_reads_environment_using_dotted_key_conversion()
  {
    clear_config_env();

    set_env_var("APP_NAME", "Vix.cpp");
    set_env_var("SERVER_PORT", "18083");
    set_env_var("APP_DEBUG", "true");

    Config config = make_config_from_current_env();

    assert(config.getString("app.name", "missing") == "Vix.cpp");
    assert(config.getString("server.port", "missing") == "18083");
    assert(config.getString("app.debug", "missing") == "true");

    assert(config.getString("missing.value", "fallback") == "fallback");
  }

  static void test_raw_config_set_has_priority_over_environment_for_get_int()
  {
    clear_config_env();

    set_env_var("APP_PORT", "3000");

    Config config = make_config_from_current_env();

    assert(config.getInt("app.port", -1) == 3000);

    config.set("app.port", 4000);

    assert(config.has("app.port") == true);
    assert(config.getInt("app.port", -1) == 4000);
  }

  static void test_raw_config_set_has_priority_over_environment_for_get_bool()
  {
    clear_config_env();

    set_env_var("APP_DEBUG", "false");

    Config config = make_config_from_current_env();

    assert(config.getBool("app.debug", true) == false);

    config.set("app.debug", true);

    assert(config.has("app.debug") == true);
    assert(config.getBool("app.debug", false) == true);
  }

  static void test_raw_config_set_has_priority_over_environment_for_get_string()
  {
    clear_config_env();

    set_env_var("APP_NAME", "env-name");

    Config config = make_config_from_current_env();

    assert(config.getString("app.name", "missing") == "env-name");

    config.set("app.name", "raw-name");

    assert(config.has("app.name") == true);
    assert(config.getString("app.name", "missing") == "raw-name");
  }

  static void test_load_config_reads_updated_environment_values()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18100");
    set_env_var("WAF_MODE", "basic");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 18100);
    assert(config.getWafMode() == "basic");

    set_env_var("SERVER_PORT", "18101");
    set_env_var("WAF_MODE", "strict");

    config.loadConfig();

    assert(config.getServerPort() == 18101);
    assert(config.getWafMode() == "strict");
  }

} // namespace

int main()
{
  test_server_env_values_override_defaults();
  test_server_env_invalid_int_values_fall_back_to_defaults();
  test_server_env_zero_and_negative_int_values_are_preserved();

  test_database_env_values_override_defaults();
  test_database_env_invalid_port_falls_back_to_default();

  test_logging_env_values_override_defaults();
  test_logging_env_invalid_values_fall_back_to_defaults();

  test_waf_env_values_override_defaults();
  test_waf_env_invalid_int_values_fall_back_to_defaults();

  test_tls_env_values_override_defaults();
  test_tls_enabled_without_files_is_not_valid();

  test_has_reads_environment_using_dotted_key_conversion();

  test_get_int_reads_environment_using_dotted_key_conversion();
  test_get_int_environment_invalid_value_returns_default();

  test_get_bool_reads_environment_using_dotted_key_conversion();
  test_get_bool_environment_common_values();
  test_get_bool_environment_invalid_value_returns_default();

  test_get_string_reads_environment_using_dotted_key_conversion();

  test_raw_config_set_has_priority_over_environment_for_get_int();
  test_raw_config_set_has_priority_over_environment_for_get_bool();
  test_raw_config_set_has_priority_over_environment_for_get_string();

  test_load_config_reads_updated_environment_values();

  return 0;
}
