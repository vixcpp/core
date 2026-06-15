/**
 *
 * @file config_set_get_test.cpp
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
#include <vix/json/json.hpp>

namespace
{
  using Config = vix::config::Config;
  using Json = vix::json::Json;

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

  static std::filesystem::path make_empty_env_path()
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("vix_config_set_get_test_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    assert(!ec);

    return dir / ".env";
  }

  static Config make_config()
  {
    clear_config_env();

    const std::filesystem::path env_path = make_empty_env_path();

    Config config{env_path};

    return config;
  }

  static void test_set_empty_key_is_ignored()
  {
    Config config = make_config();

    config.set("", "ignored");

    assert(config.has("") == false);
    assert(config.getString("", "fallback") == "fallback");
    assert(config.getInt("", 42) == 42);
    assert(config.getBool("", true) == true);
  }

  static void test_set_string_value()
  {
    Config config = make_config();

    config.set("app.name", "Vix.cpp");

    assert(config.has("app.name") == true);
    assert(config.getString("app.name", "missing") == "Vix.cpp");

    assert(config.getInt("app.name", 99) == 99);
    assert(config.getBool("app.name", false) == false);
  }

  static void test_set_integer_value()
  {
    Config config = make_config();

    config.set("app.port", 3000);

    assert(config.has("app.port") == true);
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getString("app.port", "missing") == "3000");
    assert(config.getBool("app.port", false) == true);
  }

  static void test_set_zero_integer_value()
  {
    Config config = make_config();

    config.set("app.port", 0);

    assert(config.has("app.port") == true);
    assert(config.getInt("app.port", -1) == 0);
    assert(config.getString("app.port", "missing") == "0");
    assert(config.getBool("app.port", true) == false);
  }

  static void test_set_negative_integer_value()
  {
    Config config = make_config();

    config.set("app.priority", -5);

    assert(config.has("app.priority") == true);
    assert(config.getInt("app.priority", 0) == -5);
    assert(config.getString("app.priority", "missing") == "-5");
    assert(config.getBool("app.priority", false) == true);
  }

  static void test_set_unsigned_integer_value()
  {
    Config config = make_config();

    config.set("app.workers", 8u);

    assert(config.has("app.workers") == true);
    assert(config.getInt("app.workers", -1) == 8);
    assert(config.getString("app.workers", "missing") == "8");
    assert(config.getBool("app.workers", false) == true);
  }

  static void test_set_boolean_true_value()
  {
    Config config = make_config();

    config.set("app.debug", true);

    assert(config.has("app.debug") == true);
    assert(config.getBool("app.debug", false) == true);
    assert(config.getInt("app.debug", 0) == 1);
    assert(config.getString("app.debug", "missing") == "true");
  }

  static void test_set_boolean_false_value()
  {
    Config config = make_config();

    config.set("app.debug", false);

    assert(config.has("app.debug") == true);
    assert(config.getBool("app.debug", true) == false);
    assert(config.getInt("app.debug", 9) == 0);
    assert(config.getString("app.debug", "missing") == "false");
  }

  static void test_set_float_value()
  {
    Config config = make_config();

    config.set("app.ratio", 3.75);

    assert(config.has("app.ratio") == true);
    assert(config.getInt("app.ratio", -1) == 3);

    const std::string value = config.getString("app.ratio", "missing");

    assert(value.find("3.750000") == 0);
    assert(config.getBool("app.ratio", true) == true);
  }

  static void test_set_null_value_is_present_but_uses_fallback_for_typed_accessors()
  {
    Config config = make_config();

    config.set("app.optional", nullptr);

    assert(config.has("app.optional") == true);

    assert(config.getInt("app.optional", 123) == 123);
    assert(config.getBool("app.optional", true) == true);
    assert(config.getBool("app.optional", false) == false);
    assert(config.getString("app.optional", "fallback") == "fallback");
  }

  static void test_set_array_value_is_present_but_uses_fallback_for_typed_accessors()
  {
    Config config = make_config();

    config.set("app.items", Json::array({1, 2, 3}));

    assert(config.has("app.items") == true);

    assert(config.getInt("app.items", 123) == 123);
    assert(config.getBool("app.items", false) == false);
    assert(config.getString("app.items", "fallback") == "fallback");
  }

  static void test_set_object_value_is_present_but_uses_fallback_for_typed_accessors()
  {
    Config config = make_config();

    config.set("app.meta", Json::object({{"name", "vix"}}));

    assert(config.has("app.meta") == true);

    assert(config.getInt("app.meta", 123) == 123);
    assert(config.getBool("app.meta", false) == false);
    assert(config.getString("app.meta", "fallback") == "fallback");
  }

  static void test_set_nested_dotted_keys()
  {
    Config config = make_config();

    config.set("server.http.port", 8088);
    config.set("server.http.host", "127.0.0.1");
    config.set("server.http.enabled", true);

    assert(config.has("server") == true);
    assert(config.has("server.http") == true);
    assert(config.has("server.http.port") == true);
    assert(config.has("server.http.host") == true);
    assert(config.has("server.http.enabled") == true);

    assert(config.getInt("server.http.port", -1) == 8088);
    assert(config.getString("server.http.host", "missing") == "127.0.0.1");
    assert(config.getBool("server.http.enabled", false) == true);
  }

  static void test_set_multiple_sibling_keys()
  {
    Config config = make_config();

    config.set("app.name", "vix");
    config.set("app.version", "2.6.3");
    config.set("app.debug", true);
    config.set("app.port", 3000);

    assert(config.has("app") == true);
    assert(config.has("app.name") == true);
    assert(config.has("app.version") == true);
    assert(config.has("app.debug") == true);
    assert(config.has("app.port") == true);

    assert(config.getString("app.name", "") == "vix");
    assert(config.getString("app.version", "") == "2.6.3");
    assert(config.getBool("app.debug", false) == true);
    assert(config.getInt("app.port", -1) == 3000);
  }

  static void test_set_overwrites_existing_value()
  {
    Config config = make_config();

    config.set("app.name", "old");
    assert(config.getString("app.name", "missing") == "old");

    config.set("app.name", "new");
    assert(config.getString("app.name", "missing") == "new");

    config.set("app.name", 42);
    assert(config.getInt("app.name", -1) == 42);
    assert(config.getString("app.name", "missing") == "42");
  }

  static void test_set_overwrites_nested_leaf_value()
  {
    Config config = make_config();

    config.set("app.http.port", 3000);
    assert(config.getInt("app.http.port", -1) == 3000);

    config.set("app.http.port", 4000);
    assert(config.getInt("app.http.port", -1) == 4000);

    config.set("app.http.port", "5000");
    assert(config.getInt("app.http.port", -1) == 5000);
    assert(config.getString("app.http.port", "missing") == "5000");
  }

  static void test_set_server_port_updates_accessor_and_raw_config()
  {
    Config config = make_config();

    assert(config.getServerPort() == 8080);

    config.setServerPort(9090);

    assert(config.getServerPort() == 9090);

    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 9090);
    assert(config.getString("server.port", "missing") == "9090");
    assert(config.getBool("server.port", false) == true);
  }

  static void test_set_server_port_can_set_zero_or_negative_values()
  {
    Config config = make_config();

    config.setServerPort(0);

    assert(config.getServerPort() == 0);
    assert(config.getInt("server.port", -1) == 0);
    assert(config.getBool("server.port", true) == false);

    config.setServerPort(-1);

    assert(config.getServerPort() == -1);
    assert(config.getInt("server.port", 0) == -1);
    assert(config.getString("server.port", "missing") == "-1");
  }

  static void test_set_raw_config_has_priority_over_environment()
  {
    clear_config_env();

    set_env_var("APP_NAME", "from-env");
    set_env_var("APP_PORT", "3000");
    set_env_var("APP_DEBUG", "false");

    const std::filesystem::path env_path = make_empty_env_path();
    Config config{env_path};

    assert(config.getString("app.name", "missing") == "from-env");
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getBool("app.debug", true) == false);

    config.set("app.name", "from-raw");
    config.set("app.port", 4000);
    config.set("app.debug", true);

    assert(config.getString("app.name", "missing") == "from-raw");
    assert(config.getInt("app.port", -1) == 4000);
    assert(config.getBool("app.debug", false) == true);
  }

  static void test_get_int_from_string_values()
  {
    Config config = make_config();

    config.set("values.valid", "42");
    config.set("values.negative", "-42");
    config.set("values.zero", "0");
    config.set("values.invalid", "42px");
    config.set("values.empty", "");

    assert(config.getInt("values.valid", -1) == 42);
    assert(config.getInt("values.negative", 0) == -42);
    assert(config.getInt("values.zero", -1) == 0);

    assert(config.getInt("values.invalid", 99) == 99);
    assert(config.getInt("values.empty", 77) == 77);
  }

  static void test_get_bool_from_string_values()
  {
    Config config = make_config();

    config.set("flags.true_lower", "true");
    config.set("flags.true_upper", "TRUE");
    config.set("flags.true_one", "1");
    config.set("flags.true_yes", "yes");
    config.set("flags.true_on", "on");

    config.set("flags.false_lower", "false");
    config.set("flags.false_upper", "FALSE");
    config.set("flags.false_zero", "0");
    config.set("flags.false_no", "no");
    config.set("flags.false_off", "off");

    config.set("flags.invalid", "maybe");

    assert(config.getBool("flags.true_lower", false) == true);
    assert(config.getBool("flags.true_upper", false) == true);
    assert(config.getBool("flags.true_one", false) == true);
    assert(config.getBool("flags.true_yes", false) == true);
    assert(config.getBool("flags.true_on", false) == true);

    assert(config.getBool("flags.false_lower", true) == false);
    assert(config.getBool("flags.false_upper", true) == false);
    assert(config.getBool("flags.false_zero", true) == false);
    assert(config.getBool("flags.false_no", true) == false);
    assert(config.getBool("flags.false_off", true) == false);

    assert(config.getBool("flags.invalid", true) == true);
    assert(config.getBool("flags.invalid", false) == false);
  }

  static void test_get_string_from_number_and_bool_values()
  {
    Config config = make_config();

    config.set("values.int", 123);
    config.set("values.negative", -123);
    config.set("values.unsigned", 123u);
    config.set("values.float", 1.5);
    config.set("values.true", true);
    config.set("values.false", false);

    assert(config.getString("values.int", "missing") == "123");
    assert(config.getString("values.negative", "missing") == "-123");
    assert(config.getString("values.unsigned", "missing") == "123");

    const std::string float_value = config.getString("values.float", "missing");
    assert(float_value.find("1.500000") == 0);

    assert(config.getString("values.true", "missing") == "true");
    assert(config.getString("values.false", "missing") == "false");
  }

  static void test_has_parent_nodes_created_by_set()
  {
    Config config = make_config();

    config.set("a.b.c.d", 1);

    assert(config.has("a") == true);
    assert(config.has("a.b") == true);
    assert(config.has("a.b.c") == true);
    assert(config.has("a.b.c.d") == true);

    assert(config.getInt("a.b.c.d", -1) == 1);

    assert(config.has("a.b.c.x") == false);
    assert(config.getInt("a.b.c.x", 99) == 99);
  }

  static void test_missing_nested_keys_fall_back()
  {
    Config config = make_config();

    config.set("app.name", "vix");

    assert(config.has("app.name") == true);
    assert(config.has("app.version") == false);
    assert(config.has("app.version.major") == false);

    assert(config.getString("app.version", "fallback") == "fallback");
    assert(config.getInt("app.version.major", 1) == 1);
    assert(config.getBool("app.feature.enabled", false) == false);
  }

  static void test_load_config_clears_values_created_by_set()
  {
    Config config = make_config();

    config.set("app.name", "vix");
    config.set("app.port", 3000);
    config.set("app.debug", true);

    assert(config.has("app.name") == true);
    assert(config.getString("app.name", "") == "vix");
    assert(config.getInt("app.port", -1) == 3000);
    assert(config.getBool("app.debug", false) == true);

    config.loadConfig();

    assert(config.has("app.name") == false);
    assert(config.has("app.port") == false);
    assert(config.has("app.debug") == false);

    assert(config.getString("app.name", "fallback") == "fallback");
    assert(config.getInt("app.port", 123) == 123);
    assert(config.getBool("app.debug", false) == false);
  }

  static void test_load_config_clears_set_server_port_raw_key_but_keeps_loaded_server_port()
  {
    clear_config_env();

    set_env_var("SERVER_PORT", "18080");

    const std::filesystem::path env_path = make_empty_env_path();
    Config config{env_path};

    assert(config.getServerPort() == 18080);

    config.setServerPort(19090);

    assert(config.getServerPort() == 19090);
    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 19090);

    config.loadConfig();

    assert(config.getServerPort() == 18080);

    /*
     * server.port rawConfig_ is cleared, but has()/getInt() can still see
     * SERVER_PORT through dotted-to-env lookup.
     */
    assert(config.has("server.port") == true);
    assert(config.getInt("server.port", -1) == 18080);
  }

} // namespace

int main()
{
  test_set_empty_key_is_ignored();

  test_set_string_value();

  test_set_integer_value();
  test_set_zero_integer_value();
  test_set_negative_integer_value();
  test_set_unsigned_integer_value();

  test_set_boolean_true_value();
  test_set_boolean_false_value();

  test_set_float_value();

  test_set_null_value_is_present_but_uses_fallback_for_typed_accessors();
  test_set_array_value_is_present_but_uses_fallback_for_typed_accessors();
  test_set_object_value_is_present_but_uses_fallback_for_typed_accessors();

  test_set_nested_dotted_keys();
  test_set_multiple_sibling_keys();

  test_set_overwrites_existing_value();
  test_set_overwrites_nested_leaf_value();

  test_set_server_port_updates_accessor_and_raw_config();
  test_set_server_port_can_set_zero_or_negative_values();

  test_set_raw_config_has_priority_over_environment();

  test_get_int_from_string_values();
  test_get_bool_from_string_values();
  test_get_string_from_number_and_bool_values();

  test_has_parent_nodes_created_by_set();
  test_missing_nested_keys_fall_back();

  test_load_config_clears_values_created_by_set();
  test_load_config_clears_set_server_port_raw_key_but_keeps_loaded_server_port();

  return 0;
}
