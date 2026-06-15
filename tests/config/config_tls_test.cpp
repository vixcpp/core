/**
 *
 * @file config_tls_test.cpp
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

    set_env_var("VIX_ENV_SILENT", "true");
  }

  static std::filesystem::path make_empty_env_path()
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("vix_config_tls_test_" + std::to_string(stamp));

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

  static Config make_clean_config()
  {
    clear_config_env();

    return make_config_from_current_env();
  }

  static void assert_tls_disabled_empty(const Config &config)
  {
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

  static void assert_tls_valid(
      const Config &config,
      const std::string &cert_file,
      const std::string &key_file)
  {
    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == cert_file);
    assert(config.getTlsKeyFile() == key_file);

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == cert_file);
    assert(tls.key_file == key_file);

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == true);
  }

  static void test_default_tls_values()
  {
    Config config = make_clean_config();

    assert_tls_disabled_empty(config);
  }

  static void test_tls_enabled_true_with_cert_and_key_is_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "/etc/ssl/vix/fullchain.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "/etc/ssl/vix/privkey.pem");

    Config config = make_config_from_current_env();

    assert_tls_valid(
        config,
        "/etc/ssl/vix/fullchain.pem",
        "/etc/ssl/vix/privkey.pem");
  }

  static void test_tls_enabled_one_with_cert_and_key_is_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "1");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert_tls_valid(config, "cert.pem", "key.pem");
  }

  static void test_tls_enabled_yes_with_cert_and_key_is_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "yes");
    set_env_var("SERVER_TLS_CERT_FILE", "fullchain.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "privkey.pem");

    Config config = make_config_from_current_env();

    assert_tls_valid(config, "fullchain.pem", "privkey.pem");
  }

  static void test_tls_enabled_on_with_cert_and_key_is_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "on");
    set_env_var("SERVER_TLS_CERT_FILE", "server.crt");
    set_env_var("SERVER_TLS_KEY_FILE", "server.key");

    Config config = make_config_from_current_env();

    assert_tls_valid(config, "server.crt", "server.key");
  }

  static void test_tls_enabled_false_with_cert_and_key_is_not_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "key.pem");

    assert(tls.is_enabled() == false);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == false);
  }

  static void test_tls_enabled_zero_with_cert_and_key_is_not_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "0");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "key.pem");

    assert(tls.is_enabled() == false);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == false);
  }

  static void test_tls_enabled_no_with_cert_and_key_is_not_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "no");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "key.pem");

    assert(tls.is_valid() == false);
  }

  static void test_tls_enabled_off_with_cert_and_key_is_not_valid()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "off");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "key.pem");

    assert(tls.is_valid() == false);
  }

  static void test_tls_invalid_enabled_value_falls_back_to_disabled()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "maybe");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "key.pem");

    assert(tls.is_enabled() == false);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == false);
  }

  static void test_tls_enabled_empty_value_falls_back_to_disabled()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.is_configured() == true);
    assert(tls.is_valid() == false);
  }

  static void test_tls_enabled_without_cert_and_key_is_not_configured()
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

  static void test_tls_enabled_with_cert_only_is_not_configured()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "cert.pem");
    assert(tls.key_file == "");

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_tls_enabled_with_key_only_is_not_configured()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "");
    assert(tls.key_file == "key.pem");

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_tls_disabled_with_no_files_is_not_configured()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "false");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_tls_cert_and_key_can_be_relative_paths()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "certs/dev/fullchain.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "certs/dev/privkey.pem");

    Config config = make_config_from_current_env();

    assert_tls_valid(
        config,
        "certs/dev/fullchain.pem",
        "certs/dev/privkey.pem");
  }

  static void test_tls_cert_and_key_can_be_absolute_paths()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "/etc/letsencrypt/live/example.com/fullchain.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "/etc/letsencrypt/live/example.com/privkey.pem");

    Config config = make_config_from_current_env();

    assert_tls_valid(
        config,
        "/etc/letsencrypt/live/example.com/fullchain.pem",
        "/etc/letsencrypt/live/example.com/privkey.pem");
  }

  static void test_tls_empty_cert_file_is_not_configured()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_tls_empty_key_file_is_not_configured()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.is_enabled() == true);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_tls_environment_keys_are_visible_through_dotted_accessors()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.has("server.tls.enabled") == true);
    assert(config.has("server.tls.cert.file") == true);
    assert(config.has("server.tls.key.file") == true);

    assert(config.getBool("server.tls.enabled", false) == true);
    assert(config.getString("server.tls.enabled", "missing") == "true");

    assert(config.getString("server.tls.cert.file", "missing") == "cert.pem");
    assert(config.getString("server.tls.key.file", "missing") == "key.pem");
  }

  static void test_tls_missing_dotted_keys_return_fallbacks()
  {
    Config config = make_clean_config();

    assert(config.has("server.tls.enabled") == false);
    assert(config.has("server.tls.cert.file") == false);
    assert(config.has("server.tls.key.file") == false);

    assert(config.getBool("server.tls.enabled", true) == true);
    assert(config.getBool("server.tls.enabled", false) == false);

    assert(config.getString("server.tls.cert.file", "fallback") == "fallback");
    assert(config.getString("server.tls.key.file", "fallback") == "fallback");
  }

  static void test_raw_tls_keys_do_not_change_dedicated_tls_accessors()
  {
    Config config = make_clean_config();

    config.set("server.tls.enabled", true);
    config.set("server.tls.cert.file", "raw-cert.pem");
    config.set("server.tls.key.file", "raw-key.pem");

    assert(config.has("server.tls.enabled") == true);
    assert(config.has("server.tls.cert.file") == true);
    assert(config.has("server.tls.key.file") == true);

    assert(config.getBool("server.tls.enabled", false) == true);
    assert(config.getString("server.tls.cert.file", "missing") == "raw-cert.pem");
    assert(config.getString("server.tls.key.file", "missing") == "raw-key.pem");

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "");
    assert(tls.key_file == "");
    assert(tls.is_valid() == false);
  }

  static void test_raw_tls_keys_override_environment_for_dotted_accessors_only()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("SERVER_TLS_CERT_FILE", "env-cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "env-key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "env-cert.pem");
    assert(config.getTlsKeyFile() == "env-key.pem");

    assert(config.getBool("server.tls.enabled", true) == false);
    assert(config.getString("server.tls.cert.file", "missing") == "env-cert.pem");
    assert(config.getString("server.tls.key.file", "missing") == "env-key.pem");

    config.set("server.tls.enabled", true);
    config.set("server.tls.cert.file", "raw-cert.pem");
    config.set("server.tls.key.file", "raw-key.pem");

    assert(config.getBool("server.tls.enabled", false) == true);
    assert(config.getString("server.tls.cert.file", "missing") == "raw-cert.pem");
    assert(config.getString("server.tls.key.file", "missing") == "raw-key.pem");

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "env-cert.pem");
    assert(config.getTlsKeyFile() == "env-key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "env-cert.pem");
    assert(tls.key_file == "env-key.pem");
    assert(tls.is_valid() == false);
  }

  static void test_load_config_updates_tls_values_from_environment()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("SERVER_TLS_CERT_FILE", "one-cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "one-key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "one-cert.pem");
    assert(config.getTlsKeyFile() == "one-key.pem");

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "two-cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "two-key.pem");

    config.loadConfig();

    assert_tls_valid(config, "two-cert.pem", "two-key.pem");
  }

  static void test_load_config_resets_tls_values_to_defaults_when_env_removed()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert_tls_valid(config, "cert.pem", "key.pem");

    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");

    config.loadConfig();

    assert_tls_disabled_empty(config);
  }

  static void test_load_config_clears_raw_tls_values()
  {
    Config config = make_clean_config();

    config.set("server.tls.enabled", true);
    config.set("server.tls.cert.file", "raw-cert.pem");
    config.set("server.tls.key.file", "raw-key.pem");

    assert(config.has("server.tls.enabled") == true);
    assert(config.has("server.tls.cert.file") == true);
    assert(config.has("server.tls.key.file") == true);

    config.loadConfig();

    assert(config.has("server.tls.enabled") == false);
    assert(config.has("server.tls.cert.file") == false);
    assert(config.has("server.tls.key.file") == false);

    assert(config.getBool("server.tls.enabled", false) == false);
    assert(config.getString("server.tls.cert.file", "fallback") == "fallback");
    assert(config.getString("server.tls.key.file", "fallback") == "fallback");

    assert_tls_disabled_empty(config);
  }

  static void test_tls_values_are_independent_from_server_port()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");
    set_env_var("SERVER_PORT", "18080");

    Config config = make_config_from_current_env();

    assert_tls_valid(config, "cert.pem", "key.pem");
    assert(config.getServerPort() == 18080);

    config.setServerPort(19090);

    assert(config.getServerPort() == 19090);
    assert_tls_valid(config, "cert.pem", "key.pem");
  }

  static void test_tls_values_are_independent_from_waf_values()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "1234");
    set_env_var("WAF_MAX_BODY_BYTES", "5678");

    Config config = make_config_from_current_env();

    assert_tls_valid(config, "cert.pem", "key.pem");

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 1234);
    assert(config.getWafMaxBodyBytes() == 5678);
  }

  static void test_copy_preserves_tls_values()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "copy-cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "copy-key.pem");

    Config original = make_config_from_current_env();
    Config copy = original;

    assert_tls_valid(copy, "copy-cert.pem", "copy-key.pem");
    assert_tls_valid(original, "copy-cert.pem", "copy-key.pem");
  }

  static void test_copy_assignment_preserves_tls_values()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "source-cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "source-key.pem");

    Config source = make_config_from_current_env();

    clear_config_env();

    Config target = make_config_from_current_env();

    assert_tls_disabled_empty(target);

    target = source;

    assert_tls_valid(target, "source-cert.pem", "source-key.pem");
  }

  static void test_tls_config_methods_for_manual_values()
  {
    TlsConfig disabled_empty{};

    assert(disabled_empty.enabled == false);
    assert(disabled_empty.cert_file == "");
    assert(disabled_empty.key_file == "");
    assert(disabled_empty.is_enabled() == false);
    assert(disabled_empty.is_configured() == false);
    assert(disabled_empty.is_valid() == false);

    TlsConfig disabled_configured{
        .enabled = false,
        .cert_file = "cert.pem",
        .key_file = "key.pem",
    };

    assert(disabled_configured.is_enabled() == false);
    assert(disabled_configured.is_configured() == true);
    assert(disabled_configured.is_valid() == false);

    TlsConfig enabled_missing_cert{
        .enabled = true,
        .cert_file = "",
        .key_file = "key.pem",
    };

    assert(enabled_missing_cert.is_enabled() == true);
    assert(enabled_missing_cert.is_configured() == false);
    assert(enabled_missing_cert.is_valid() == false);

    TlsConfig enabled_missing_key{
        .enabled = true,
        .cert_file = "cert.pem",
        .key_file = "",
    };

    assert(enabled_missing_key.is_enabled() == true);
    assert(enabled_missing_key.is_configured() == false);
    assert(enabled_missing_key.is_valid() == false);

    TlsConfig enabled_configured{
        .enabled = true,
        .cert_file = "cert.pem",
        .key_file = "key.pem",
    };

    assert(enabled_configured.is_enabled() == true);
    assert(enabled_configured.is_configured() == true);
    assert(enabled_configured.is_valid() == true);
  }

} // namespace

int main()
{
  test_default_tls_values();

  test_tls_enabled_true_with_cert_and_key_is_valid();
  test_tls_enabled_one_with_cert_and_key_is_valid();
  test_tls_enabled_yes_with_cert_and_key_is_valid();
  test_tls_enabled_on_with_cert_and_key_is_valid();

  test_tls_enabled_false_with_cert_and_key_is_not_valid();
  test_tls_enabled_zero_with_cert_and_key_is_not_valid();
  test_tls_enabled_no_with_cert_and_key_is_not_valid();
  test_tls_enabled_off_with_cert_and_key_is_not_valid();

  test_tls_invalid_enabled_value_falls_back_to_disabled();
  test_tls_enabled_empty_value_falls_back_to_disabled();

  test_tls_enabled_without_cert_and_key_is_not_configured();
  test_tls_enabled_with_cert_only_is_not_configured();
  test_tls_enabled_with_key_only_is_not_configured();
  test_tls_disabled_with_no_files_is_not_configured();

  test_tls_cert_and_key_can_be_relative_paths();
  test_tls_cert_and_key_can_be_absolute_paths();

  test_tls_empty_cert_file_is_not_configured();
  test_tls_empty_key_file_is_not_configured();

  test_tls_environment_keys_are_visible_through_dotted_accessors();
  test_tls_missing_dotted_keys_return_fallbacks();

  test_raw_tls_keys_do_not_change_dedicated_tls_accessors();
  test_raw_tls_keys_override_environment_for_dotted_accessors_only();

  test_load_config_updates_tls_values_from_environment();
  test_load_config_resets_tls_values_to_defaults_when_env_removed();
  test_load_config_clears_raw_tls_values();

  test_tls_values_are_independent_from_server_port();
  test_tls_values_are_independent_from_waf_values();

  test_copy_preserves_tls_values();
  test_copy_assignment_preserves_tls_values();

  test_tls_config_methods_for_manual_values();

  return 0;
}
