/**
 *
 * @file config_reload_test.cpp
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
    unset_env_var("APP_PORT");
    unset_env_var("APP_DEBUG");
    unset_env_var("APP_VERSION");
    unset_env_var("FEATURE_ENABLED");
    unset_env_var("FEATURE_COUNT");

    set_env_var("VIX_ENV_SILENT", "true");
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

  static std::filesystem::path make_empty_env_path()
  {
    const std::filesystem::path dir =
        make_temp_dir("vix_config_reload_test_empty");

    return dir / ".env";
  }

  static std::filesystem::path make_env_file(const std::string &content)
  {
    const std::filesystem::path dir =
        make_temp_dir("vix_config_reload_test_file");

    const std::filesystem::path path = dir / ".env";

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);

    out << content;
    out.close();

    assert(std::filesystem::exists(path));

    return path;
  }

  static void overwrite_file(const std::filesystem::path &path, const std::string &content)
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);

    out << content;
    out.close();

    assert(std::filesystem::exists(path));
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

  static void test_load_config_keeps_defaults_when_environment_is_empty()
  {
    Config config = make_clean_config();

    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");

    config.loadConfig();

    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);

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

  static void test_load_config_reads_updated_server_environment()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");
    set_env_var("SERVER_REQUEST_TIMEOUT", "3000");
    set_env_var("SERVER_IO_THREADS", "2");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_BENCH_MODE", "false");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 18080);
    assert(config.getRequestTimeout() == 3000);
    assert(config.getIOThreads() == 2);
    assert(config.getSessionTimeoutSec() == 30);
    assert(config.isBenchMode() == false);

    set_env_var("SERVER_PORT", "18081");
    set_env_var("SERVER_REQUEST_TIMEOUT", "4000");
    set_env_var("SERVER_IO_THREADS", "4");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "60");
    set_env_var("SERVER_BENCH_MODE", "true");

    config.loadConfig();

    assert(config.getServerPort() == 18081);
    assert(config.getRequestTimeout() == 4000);
    assert(config.getIOThreads() == 4);
    assert(config.getSessionTimeoutSec() == 60);
    assert(config.isBenchMode() == true);
  }

  static void test_load_config_resets_server_environment_values_when_removed()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");
    set_env_var("SERVER_REQUEST_TIMEOUT", "3000");
    set_env_var("SERVER_IO_THREADS", "2");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_BENCH_MODE", "true");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 18080);
    assert(config.getRequestTimeout() == 3000);
    assert(config.getIOThreads() == 2);
    assert(config.getSessionTimeoutSec() == 30);
    assert(config.isBenchMode() == true);

    unset_env_var("SERVER_PORT");
    unset_env_var("SERVER_REQUEST_TIMEOUT");
    unset_env_var("SERVER_IO_THREADS");
    unset_env_var("SERVER_SESSION_TIMEOUT_SEC");
    unset_env_var("SERVER_BENCH_MODE");

    config.loadConfig();

    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);

#if defined(VIX_BENCH_MODE)
    assert(config.isBenchMode() == true);
#else
    assert(config.isBenchMode() == false);
#endif
  }

  static void test_load_config_reads_updated_database_environment()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "db-one");
    set_env_var("DATABASE_DEFAULT_USER", "user-one");
    set_env_var("DATABASE_DEFAULT_NAME", "name-one");
    set_env_var("DATABASE_DEFAULT_PORT", "1111");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "db-one");
    assert(config.getDbUser() == "user-one");
    assert(config.getDbName() == "name-one");
    assert(config.getDbPort() == 1111);

    set_env_var("DATABASE_DEFAULT_HOST", "db-two");
    set_env_var("DATABASE_DEFAULT_USER", "user-two");
    set_env_var("DATABASE_DEFAULT_NAME", "name-two");
    set_env_var("DATABASE_DEFAULT_PORT", "2222");

    config.loadConfig();

    assert(config.getDbHost() == "db-two");
    assert(config.getDbUser() == "user-two");
    assert(config.getDbName() == "name-two");
    assert(config.getDbPort() == 2222);
  }

  static void test_load_config_resets_database_values_when_environment_is_removed()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "db-one");
    set_env_var("DATABASE_DEFAULT_USER", "user-one");
    set_env_var("DATABASE_DEFAULT_NAME", "name-one");
    set_env_var("DATABASE_DEFAULT_PORT", "1111");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "db-one");
    assert(config.getDbUser() == "user-one");
    assert(config.getDbName() == "name-one");
    assert(config.getDbPort() == 1111);

    unset_env_var("DATABASE_DEFAULT_HOST");
    unset_env_var("DATABASE_DEFAULT_USER");
    unset_env_var("DATABASE_DEFAULT_NAME");
    unset_env_var("DATABASE_DEFAULT_PORT");

    config.loadConfig();

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
  }

  static void test_load_config_reads_updated_waf_environment()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "basic");
    set_env_var("WAF_MAX_TARGET_LEN", "1000");
    set_env_var("WAF_MAX_BODY_BYTES", "2000");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 1000);
    assert(config.getWafMaxBodyBytes() == 2000);

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "3000");
    set_env_var("WAF_MAX_BODY_BYTES", "4000");

    config.loadConfig();

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 3000);
    assert(config.getWafMaxBodyBytes() == 4000);
  }

  static void test_load_config_resets_waf_values_when_environment_is_removed()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "3000");
    set_env_var("WAF_MAX_BODY_BYTES", "4000");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 3000);
    assert(config.getWafMaxBodyBytes() == 4000);

    unset_env_var("WAF_MODE");
    unset_env_var("WAF_MAX_TARGET_LEN");
    unset_env_var("WAF_MAX_BODY_BYTES");

    config.loadConfig();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_load_config_reads_updated_logging_environment()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "true");
    set_env_var("LOGGING_QUEUE_MAX", "1000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "true");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 1000);
    assert(config.getLogDropOnOverflow() == true);

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "2000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    config.loadConfig();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 2000);
    assert(config.getLogDropOnOverflow() == false);
  }

  static void test_load_config_resets_logging_values_when_environment_is_removed()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "3000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 3000);
    assert(config.getLogDropOnOverflow() == false);

    unset_env_var("LOGGING_ASYNC");
    unset_env_var("LOGGING_QUEUE_MAX");
    unset_env_var("LOGGING_DROP_ON_OVERFLOW");

    config.loadConfig();

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_load_config_reads_updated_tls_environment()
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

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "two-cert.pem");
    assert(config.getTlsKeyFile() == "two-key.pem");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == true);
    assert(tls.cert_file == "two-cert.pem");
    assert(tls.key_file == "two-key.pem");
    assert(tls.is_valid() == true);
  }

  static void test_load_config_resets_tls_values_when_environment_is_removed()
  {
    clear_config_env();

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");

    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");

    config.loadConfig();

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");

    const TlsConfig tls = config.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "");
    assert(tls.key_file == "");
    assert(tls.is_valid() == false);
  }

  static void test_load_config_clears_raw_values_set_by_set()
  {
    Config config = make_clean_config();

    config.set("app.name", "vix");
    config.set("app.port", 3000);
    config.set("app.debug", true);
    config.set("feature.enabled", true);

    assert(config.has("app.name") == true);
    assert(config.has("app.port") == true);
    assert(config.has("app.debug") == true);
    assert(config.has("feature.enabled") == true);

    assert(config.getString("app.name", "missing") == "vix");
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getBool("app.debug", false) == true);
    assert(config.getBool("feature.enabled", false) == true);

    config.loadConfig();

    assert(config.has("app.name") == false);
    assert(config.has("app.port") == false);
    assert(config.has("app.debug") == false);
    assert(config.has("feature.enabled") == false);

    assert(config.getString("app.name", "fallback") == "fallback");
    assert(config.getInt("app.port", 123) == 123);
    assert(config.getBool("app.debug", false) == false);
    assert(config.getBool("feature.enabled", false) == false);
  }

  static void test_load_config_clears_raw_parent_objects()
  {
    Config config = make_clean_config();

    config.set("server.http.port", 8088);
    config.set("server.http.host", "127.0.0.1");
    config.set("server.http.enabled", true);

    assert(config.has("server") == true);
    assert(config.has("server.http") == true);
    assert(config.has("server.http.port") == true);
    assert(config.has("server.http.host") == true);
    assert(config.has("server.http.enabled") == true);

    config.loadConfig();

    assert(config.has("server") == false);
    assert(config.has("server.http") == false);
    assert(config.has("server.http.port") == false);
    assert(config.has("server.http.host") == false);
    assert(config.has("server.http.enabled") == false);
  }

  static void test_load_config_clears_raw_server_port_but_preserves_loaded_server_port()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 18080);
    assert(config.getInt("server.port", -1) == 18080);

    config.setServerPort(19090);

    assert(config.getServerPort() == 19090);
    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 19090);

    config.loadConfig();

    assert(config.getServerPort() == 18080);

    /*
     * rawConfig_ was cleared, but SERVER_PORT remains visible through
     * dotted-to-environment lookup.
     */
    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 18080);
  }

  static void test_load_config_replaces_raw_values_with_environment_values()
  {
    clear_config_env();

    set_env_var("APP_NAME", "from-env");
    set_env_var("APP_PORT", "3000");
    set_env_var("APP_DEBUG", "false");

    Config config = make_config_from_current_env();

    assert(config.getString("app.name", "missing") == "from-env");
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getBool("app.debug", true) == false);

    config.set("app.name", "from-raw");
    config.set("app.port", 4000);
    config.set("app.debug", true);

    assert(config.getString("app.name", "missing") == "from-raw");
    assert(config.getInt("app.port", -1) == 4000);
    assert(config.getBool("app.debug", false) == true);

    config.loadConfig();

    assert(config.getString("app.name", "missing") == "from-env");
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getBool("app.debug", true) == false);
  }

  static void test_repeated_load_config_is_idempotent_with_same_environment()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");
    set_env_var("WAF_MODE", "strict");
    set_env_var("LOGGING_QUEUE_MAX", "1234");
    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    for (int i = 0; i < 5; ++i)
    {
      config.loadConfig();

      assert(config.getServerPort() == 18080);
      assert(config.getWafMode() == "strict");
      assert(config.getLogQueueMax() == 1234);

      assert(config.isTlsEnabled() == true);
      assert(config.getTlsCertFile() == "cert.pem");
      assert(config.getTlsKeyFile() == "key.pem");
    }
  }

  static void test_repeated_load_config_tracks_environment_changes()
  {
    clear_config_env();

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 8080);
    assert(config.getWafMode() == "basic");
    assert(config.getLogQueueMax() == 20000);

    set_env_var("SERVER_PORT", "18080");
    set_env_var("WAF_MODE", "strict");
    set_env_var("LOGGING_QUEUE_MAX", "1000");

    config.loadConfig();

    assert(config.getServerPort() == 18080);
    assert(config.getWafMode() == "strict");
    assert(config.getLogQueueMax() == 1000);

    set_env_var("SERVER_PORT", "18081");
    set_env_var("WAF_MODE", "off");
    set_env_var("LOGGING_QUEUE_MAX", "2000");

    config.loadConfig();

    assert(config.getServerPort() == 18081);
    assert(config.getWafMode() == "off");
    assert(config.getLogQueueMax() == 2000);

    unset_env_var("SERVER_PORT");
    unset_env_var("WAF_MODE");
    unset_env_var("LOGGING_QUEUE_MAX");

    config.loadConfig();

    assert(config.getServerPort() == 8080);
    assert(config.getWafMode() == "basic");
    assert(config.getLogQueueMax() == 20000);
  }

  static void test_invalid_environment_values_on_reload_fall_back_to_defaults()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");
    set_env_var("WAF_MAX_TARGET_LEN", "1234");
    set_env_var("LOGGING_QUEUE_MAX", "5678");
    set_env_var("SERVER_TLS_ENABLED", "true");

    Config config = make_config_from_current_env();

    assert(config.getServerPort() == 18080);
    assert(config.getWafMaxTargetLen() == 1234);
    assert(config.getLogQueueMax() == 5678);
    assert(config.isTlsEnabled() == true);

    set_env_var("SERVER_PORT", "bad-port");
    set_env_var("WAF_MAX_TARGET_LEN", "bad-target");
    set_env_var("LOGGING_QUEUE_MAX", "bad-queue");
    set_env_var("SERVER_TLS_ENABLED", "maybe");

    config.loadConfig();

    assert(config.getServerPort() == 8080);
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getLogQueueMax() == 20000);
    assert(config.isTlsEnabled() == false);
  }

  static void test_load_config_from_env_file_on_construction()
  {
    clear_config_env();

    const std::filesystem::path env_file = make_env_file(
        "SERVER_PORT=18100\n"
        "SERVER_REQUEST_TIMEOUT=5100\n"
        "DATABASE_DEFAULT_HOST=file-db\n"
        "DATABASE_DEFAULT_USER=file-user\n"
        "DATABASE_DEFAULT_NAME=file-name\n"
        "DATABASE_DEFAULT_PORT=4545\n"
        "WAF_MODE=strict\n"
        "WAF_MAX_TARGET_LEN=6000\n"
        "WAF_MAX_BODY_BYTES=7000\n"
        "LOGGING_ASYNC=false\n"
        "LOGGING_QUEUE_MAX=8000\n"
        "LOGGING_DROP_ON_OVERFLOW=false\n"
        "SERVER_TLS_ENABLED=true\n"
        "SERVER_TLS_CERT_FILE=file-cert.pem\n"
        "SERVER_TLS_KEY_FILE=file-key.pem\n");

    Config config{env_file};

    assert(config.getServerPort() == 18100);
    assert(config.getRequestTimeout() == 5100);

    assert(config.getDbHost() == "file-db");
    assert(config.getDbUser() == "file-user");
    assert(config.getDbName() == "file-name");
    assert(config.getDbPort() == 4545);

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 6000);
    assert(config.getWafMaxBodyBytes() == 7000);

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 8000);
    assert(config.getLogDropOnOverflow() == false);

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "file-cert.pem");
    assert(config.getTlsKeyFile() == "file-key.pem");
  }

  static void test_load_config_from_env_file_after_file_changes()
  {
    clear_config_env();

    const std::filesystem::path env_file = make_env_file(
        "SERVER_PORT=18110\n"
        "WAF_MODE=basic\n"
        "LOGGING_QUEUE_MAX=1111\n");

    Config config{env_file};

    assert(config.getServerPort() == 18110);
    assert(config.getWafMode() == "basic");
    assert(config.getLogQueueMax() == 1111);

    clear_config_env();

    overwrite_file(
        env_file,
        "SERVER_PORT=18111\n"
        "WAF_MODE=strict\n"
        "LOGGING_QUEUE_MAX=2222\n");

    config.loadConfig();

    assert(config.getServerPort() == 18111);
    assert(config.getWafMode() == "strict");
    assert(config.getLogQueueMax() == 2222);
  }

  static void test_config_file_reload_has_priority_over_late_environment_override()
  {
    clear_config_env();

    const std::filesystem::path env_file = make_env_file(
        "SERVER_PORT=18120\n"
        "WAF_MODE=basic\n");

    Config config{env_file};

    assert(config.getServerPort() == 18120);
    assert(config.getWafMode() == "basic");

    set_env_var("SERVER_PORT", "18121");
    set_env_var("WAF_MODE", "strict");

    config.loadConfig();

    /*
     * loadConfig() reloads the config file. The file remains the source of
     * truth for this Config instance.
     */
    assert(config.getServerPort() == 18120);
    assert(config.getWafMode() == "basic");
  }
} // namespace

int main()
{
  test_load_config_keeps_defaults_when_environment_is_empty();

  test_load_config_reads_updated_server_environment();
  test_load_config_resets_server_environment_values_when_removed();

  test_load_config_reads_updated_database_environment();
  test_load_config_resets_database_values_when_environment_is_removed();

  test_load_config_reads_updated_waf_environment();
  test_load_config_resets_waf_values_when_environment_is_removed();

  test_load_config_reads_updated_logging_environment();
  test_load_config_resets_logging_values_when_environment_is_removed();

  test_load_config_reads_updated_tls_environment();
  test_load_config_resets_tls_values_when_environment_is_removed();

  test_load_config_clears_raw_values_set_by_set();
  test_load_config_clears_raw_parent_objects();

  test_load_config_clears_raw_server_port_but_preserves_loaded_server_port();
  test_load_config_replaces_raw_values_with_environment_values();

  test_repeated_load_config_is_idempotent_with_same_environment();
  test_repeated_load_config_tracks_environment_changes();

  test_invalid_environment_values_on_reload_fall_back_to_defaults();

  test_load_config_from_env_file_on_construction();
  test_load_config_from_env_file_after_file_changes();
  test_config_file_reload_has_priority_over_late_environment_override();

  return 0;
}
