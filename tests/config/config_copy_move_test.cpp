/**
 *
 * @file config_copy_move_test.cpp
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
#include <utility>
#include <fstream>

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
        ("vix_config_copy_move_test_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    assert(!ec);

    return dir / ".env";
  }

  static std::filesystem::path make_env_file(const std::string &content)
  {
    const std::filesystem::path path = make_empty_env_path();

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);

    out << content;
    out.close();

    assert(std::filesystem::exists(path));

    return path;
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

  static Config make_populated_config()
  {
    Config config = make_clean_config();

    config.setServerPort(18080);

    config.set("app.name", "Vix.cpp");
    config.set("app.port", 3000);
    config.set("app.debug", true);
    config.set("feature.enabled", true);
    config.set("feature.count", 7);

    config.set("copy.server.timeout", 4500);
    config.set("copy.io.threads", 4);
    config.set("copy.session.timeout.sec", 45);
    config.set("copy.bench.mode", true);

    config.set("copy.logging.async", false);
    config.set("copy.logging.queue.max", 4096);
    config.set("copy.logging.drop.on.overflow", false);

    config.set("copy.waf.mode", "strict");
    config.set("copy.waf.max.target.len", 2048);
    config.set("copy.waf.max.body.bytes", 65536);

    config.set("copy.tls.enabled", true);
    config.set("copy.tls.cert.file", "cert.pem");
    config.set("copy.tls.key.file", "key.pem");

    return config;
  }

  static void assert_populated_dedicated_values(const Config &config)
  {
    /*
     * Dedicated environment loading is covered by:
     * - config_database_test
     * - config_waf_test
     * - config_tls_test
     * - config_logging_test
     *
     * This copy/move test only checks values that are stable after copy/move:
     * - explicit setServerPort()
     * - rawConfig_ values created with set()
     */
    assert(config.getServerPort() == 18080);
    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 18080);
  }

  static void assert_populated_raw_values(const Config &config)
  {
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

    assert(config.getInt("copy.server.timeout", -1) == 4500);
    assert(config.getInt("copy.io.threads", -1) == 4);
    assert(config.getInt("copy.session.timeout.sec", -1) == 45);
    assert(config.getBool("copy.bench.mode", false) == true);

    assert(config.getBool("copy.logging.async", true) == false);
    assert(config.getInt("copy.logging.queue.max", -1) == 4096);
    assert(config.getBool("copy.logging.drop.on.overflow", true) == false);

    assert(config.getString("copy.waf.mode", "missing") == "strict");
    assert(config.getInt("copy.waf.max.target.len", -1) == 2048);
    assert(config.getInt("copy.waf.max.body.bytes", -1) == 65536);

    assert(config.getBool("copy.tls.enabled", false) == true);
    assert(config.getString("copy.tls.cert.file", "missing") == "cert.pem");
    assert(config.getString("copy.tls.key.file", "missing") == "key.pem");
  }

  static void assert_populated_config(const Config &config)
  {
    assert_populated_dedicated_values(config);
    assert_populated_raw_values(config);
  }

  static void assert_default_dedicated_values(const Config &config)
  {
    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);

    assert(config.getServerPort() == 8080);
    assert(config.getRequestTimeout() == 2000);
    assert(config.getIOThreads() == 0);
    assert(config.getSessionTimeoutSec() == 20);

#if defined(VIX_BENCH_MODE)
    assert(config.isBenchMode() == true);
#else
    assert(config.isBenchMode() == false);
#endif

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);

    assert(config.isTlsEnabled() == false);
    assert(config.getTlsCertFile() == "");
    assert(config.getTlsKeyFile() == "");
  }

  static void test_config_copy_move_type_traits()
  {
    static_assert(std::is_copy_constructible_v<Config>);
    static_assert(std::is_copy_assignable_v<Config>);

    static_assert(std::is_move_constructible_v<Config>);
    static_assert(std::is_move_assignable_v<Config>);

    static_assert(std::is_destructible_v<Config>);

    static_assert(noexcept(Config(std::declval<Config &&>())));
    static_assert(noexcept(std::declval<Config &>() = std::declval<Config &&>()));
  }

  static void test_copy_constructor_preserves_default_values()
  {
    Config original = make_clean_config();

    Config copy = original;

    assert_default_dedicated_values(original);
    assert_default_dedicated_values(copy);
  }

  static void test_copy_constructor_preserves_populated_values()
  {
    Config original = make_populated_config();

    Config copy = original;

    assert_populated_config(original);
    assert_populated_config(copy);
  }

  static void test_copy_constructor_creates_independent_raw_config()
  {
    Config original = make_clean_config();

    original.set("app.name", "original");
    original.set("app.port", 3000);

    Config copy = original;

    assert(copy.getString("app.name", "missing") == "original");
    assert(copy.getInt("app.port", -1) == 3000);

    copy.set("app.name", "copy");
    copy.set("app.port", 4000);

    assert(original.getString("app.name", "missing") == "original");
    assert(original.getInt("app.port", -1) == 3000);

    assert(copy.getString("app.name", "missing") == "copy");
    assert(copy.getInt("app.port", -1) == 4000);
  }

  static void test_copy_constructor_preserves_set_server_port()
  {
    Config original = make_clean_config();

    original.setServerPort(19090);

    assert(original.getServerPort() == 19090);
    assert(original.getInt("server.port", -1) == 19090);

    Config copy = original;

    assert(copy.getServerPort() == 19090);
    assert(copy.getInt("server.port", -1) == 19090);

    copy.setServerPort(20000);

    assert(original.getServerPort() == 19090);
    assert(original.getInt("server.port", -1) == 19090);

    assert(copy.getServerPort() == 20000);
    assert(copy.getInt("server.port", -1) == 20000);
  }

  static void test_copy_assignment_preserves_default_values()
  {
    Config source = make_clean_config();

    Config target = make_populated_config();

    assert_populated_config(target);

    target = source;

    assert_default_dedicated_values(source);
    assert_default_dedicated_values(target);

    assert(target.has("app.name") == false);
    assert(target.has("app.port") == false);
    assert(target.getString("app.name", "fallback") == "fallback");
    assert(target.getInt("app.port", 123) == 123);
  }

  static void test_copy_assignment_preserves_populated_values()
  {
    Config source = make_populated_config();

    Config target = make_clean_config();

    assert_default_dedicated_values(target);

    target = source;

    assert_populated_config(source);
    assert_populated_config(target);
  }

  static void test_copy_assignment_replaces_existing_raw_config()
  {
    Config source = make_clean_config();

    source.set("source.name", "source");
    source.set("source.port", 1111);

    Config target = make_clean_config();

    target.set("target.name", "target");
    target.set("target.port", 2222);

    assert(target.has("target.name") == true);
    assert(target.has("target.port") == true);

    target = source;

    assert(target.has("source.name") == true);
    assert(target.has("source.port") == true);
    assert(target.getString("source.name", "missing") == "source");
    assert(target.getInt("source.port", -1) == 1111);

    assert(target.has("target.name") == false);
    assert(target.has("target.port") == false);
  }

  static void test_copy_assignment_self_assignment_is_safe()
  {
    Config config = make_populated_config();

    Config &ref = config;
    config = ref;

    assert_populated_config(config);
  }

  static void test_move_constructor_preserves_default_values()
  {
    Config source = make_clean_config();

    Config moved = std::move(source);

    assert_default_dedicated_values(moved);
  }

  static void test_move_constructor_preserves_populated_values()
  {
    Config source = make_populated_config();

    Config moved = std::move(source);

    assert_populated_config(moved);
  }

  static void test_move_constructor_preserves_raw_config()
  {
    Config source = make_clean_config();

    source.set("app.name", "moved");
    source.set("app.port", 3000);
    source.set("app.debug", true);

    Config moved = std::move(source);

    assert(moved.has("app.name") == true);
    assert(moved.has("app.port") == true);
    assert(moved.has("app.debug") == true);

    assert(moved.getString("app.name", "missing") == "moved");
    assert(moved.getInt("app.port", -1) == 3000);
    assert(moved.getBool("app.debug", false) == true);
  }

  static void test_move_constructor_preserves_set_server_port()
  {
    Config source = make_clean_config();

    source.setServerPort(19090);

    assert(source.getServerPort() == 19090);

    Config moved = std::move(source);

    assert(moved.getServerPort() == 19090);
    assert(moved.has("server.port") == true);
    assert(moved.getInt("server.port", -1) == 19090);
  }

  static void test_move_assignment_preserves_default_values()
  {
    Config source = make_clean_config();

    Config target = make_populated_config();

    assert_populated_config(target);

    target = std::move(source);

    assert_default_dedicated_values(target);

    assert(target.has("app.name") == false);
    assert(target.has("app.port") == false);
  }

  static void test_move_assignment_preserves_populated_values()
  {
    Config source = make_populated_config();

    Config target = make_clean_config();

    assert_default_dedicated_values(target);

    target = std::move(source);

    assert_populated_config(target);
  }

  static void test_move_assignment_replaces_existing_raw_config()
  {
    Config source = make_clean_config();

    source.set("source.name", "source");
    source.set("source.port", 1111);

    Config target = make_clean_config();

    target.set("target.name", "target");
    target.set("target.port", 2222);

    target = std::move(source);

    assert(target.has("source.name") == true);
    assert(target.has("source.port") == true);
    assert(target.getString("source.name", "missing") == "source");
    assert(target.getInt("source.port", -1) == 1111);

    assert(target.has("target.name") == false);
    assert(target.has("target.port") == false);
  }

  static void test_move_assignment_self_assignment_is_safe()
  {
    Config config = make_populated_config();

    Config &ref = config;
    config = std::move(ref);

    assert_populated_config(config);
  }

  static void test_copy_then_load_config_uses_copied_config_path()
  {
    clear_config_env();

    const std::filesystem::path env_path = make_empty_env_path();

    {
      std::ofstream out(env_path, std::ios::binary | std::ios::trunc);
      assert(out);
      out << "SERVER_PORT=18100\n";
      out << "WAF_MODE=basic\n";
      out.close();
    }

    Config original{env_path};

    assert(original.getServerPort() == 18100);
    assert(original.getWafMode() == "basic");

    Config copy = original;

    assert(copy.getServerPort() == 18100);
    assert(copy.getWafMode() == "basic");

    clear_config_env();

    {
      std::ofstream out(env_path, std::ios::binary | std::ios::trunc);
      assert(out);
      out << "SERVER_PORT=18101\n";
      out << "WAF_MODE=strict\n";
      out.close();
    }

    copy.loadConfig();

    assert(copy.getServerPort() == 18101);
    assert(copy.getWafMode() == "strict");

    assert(original.getServerPort() == 18100);
    assert(original.getWafMode() == "basic");

    clear_config_env();

    original.loadConfig();

    assert(original.getServerPort() == 18101);
    assert(original.getWafMode() == "strict");
  }

  static void test_move_then_load_config_uses_moved_config_path()
  {
    clear_config_env();

    const std::filesystem::path env_path = make_empty_env_path();

    {
      std::ofstream out(env_path, std::ios::binary | std::ios::trunc);
      assert(out);
      out << "SERVER_PORT=18200\n";
      out << "WAF_MODE=basic\n";
      out.close();
    }

    Config source{env_path};

    assert(source.getServerPort() == 18200);
    assert(source.getWafMode() == "basic");

    Config moved = std::move(source);

    assert(moved.getServerPort() == 18200);
    assert(moved.getWafMode() == "basic");

    clear_config_env();

    {
      std::ofstream out(env_path, std::ios::binary | std::ios::trunc);
      assert(out);
      out << "SERVER_PORT=18201\n";
      out << "WAF_MODE=strict\n";
      out.close();
    }

    moved.loadConfig();

    assert(moved.getServerPort() == 18201);
    assert(moved.getWafMode() == "strict");
  }

  static void test_copy_preserves_tls_config_snapshot()
  {
    Config original = make_clean_config();

    Config copy = original;

    const TlsConfig original_tls = original.getTlsConfig();
    const TlsConfig copy_tls = copy.getTlsConfig();

    assert(original_tls.enabled == false);
    assert(copy_tls.enabled == false);

    assert(original_tls.cert_file == "");
    assert(copy_tls.cert_file == "");

    assert(original_tls.key_file == "");
    assert(copy_tls.key_file == "");

    assert(original_tls.is_valid() == false);
    assert(copy_tls.is_valid() == false);
  }

  static void test_move_preserves_tls_config_snapshot()
  {
    Config source = make_clean_config();

    Config moved = std::move(source);

    const TlsConfig tls = moved.getTlsConfig();

    assert(tls.enabled == false);
    assert(tls.cert_file == "");
    assert(tls.key_file == "");
    assert(tls.is_enabled() == false);
    assert(tls.is_configured() == false);
    assert(tls.is_valid() == false);
  }

  static void test_copy_preserves_parent_raw_nodes()
  {
    Config original = make_clean_config();

    original.set("a.b.c", 1);
    original.set("a.b.name", "node");

    Config copy = original;

    assert(copy.has("a") == true);
    assert(copy.has("a.b") == true);
    assert(copy.has("a.b.c") == true);
    assert(copy.has("a.b.name") == true);

    assert(copy.getInt("a.b.c", -1) == 1);
    assert(copy.getString("a.b.name", "missing") == "node");
  }

  static void test_move_preserves_parent_raw_nodes()
  {
    Config source = make_clean_config();

    source.set("a.b.c", 1);
    source.set("a.b.name", "node");

    Config moved = std::move(source);

    assert(moved.has("a") == true);
    assert(moved.has("a.b") == true);
    assert(moved.has("a.b.c") == true);
    assert(moved.has("a.b.name") == true);

    assert(moved.getInt("a.b.c", -1) == 1);
    assert(moved.getString("a.b.name", "missing") == "node");
  }

  static void test_copy_can_be_modified_without_modifying_original()
  {
    Config original = make_populated_config();

    Config copy = original;

    copy.set("app.name", "copy-name");
    copy.set("app.port", 4000);
    copy.setServerPort(20000);

    assert(original.getString("app.name", "missing") == "Vix.cpp");
    assert(original.getInt("app.port", -1) == 3000);
    assert(original.getServerPort() == 18080);

    assert(copy.getString("app.name", "missing") == "copy-name");
    assert(copy.getInt("app.port", -1) == 4000);
    assert(copy.getServerPort() == 20000);
  }

  static void test_assignment_target_can_be_modified_without_modifying_source()
  {
    Config source = make_populated_config();

    Config target = make_clean_config();

    target = source;

    target.set("app.name", "target-name");
    target.set("app.port", 5000);
    target.setServerPort(21000);

    assert(source.getString("app.name", "missing") == "Vix.cpp");
    assert(source.getInt("app.port", -1) == 3000);
    assert(source.getServerPort() == 18080);

    assert(target.getString("app.name", "missing") == "target-name");
    assert(target.getInt("app.port", -1) == 5000);
    assert(target.getServerPort() == 21000);
  }

} // namespace

int main()
{
  test_config_copy_move_type_traits();

  test_copy_constructor_preserves_default_values();
  test_copy_constructor_preserves_populated_values();
  test_copy_constructor_creates_independent_raw_config();
  test_copy_constructor_preserves_set_server_port();

  test_copy_assignment_preserves_default_values();
  test_copy_assignment_preserves_populated_values();
  test_copy_assignment_replaces_existing_raw_config();
  test_copy_assignment_self_assignment_is_safe();

  test_move_constructor_preserves_default_values();
  test_move_constructor_preserves_populated_values();
  test_move_constructor_preserves_raw_config();
  test_move_constructor_preserves_set_server_port();

  test_move_assignment_preserves_default_values();
  test_move_assignment_preserves_populated_values();
  test_move_assignment_replaces_existing_raw_config();
  test_move_assignment_self_assignment_is_safe();

  test_copy_then_load_config_uses_copied_config_path();
  test_move_then_load_config_uses_moved_config_path();

  test_copy_preserves_tls_config_snapshot();
  test_move_preserves_tls_config_snapshot();

  test_copy_preserves_parent_raw_nodes();
  test_move_preserves_parent_raw_nodes();

  test_copy_can_be_modified_without_modifying_original();
  test_assignment_target_can_be_modified_without_modifying_source();

  return 0;
}
