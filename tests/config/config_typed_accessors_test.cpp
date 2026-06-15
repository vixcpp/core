/**
 *
 * @file config_typed_accessors_test.cpp
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
#include <cstdint>
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

    unset_env_var("APP_INT");
    unset_env_var("APP_BOOL");
    unset_env_var("APP_STRING");
    unset_env_var("APP_PORT");
    unset_env_var("APP_DEBUG");
    unset_env_var("APP_NAME");
    unset_env_var("FEATURE_ENABLED");
    unset_env_var("FEATURE_COUNT");
    unset_env_var("FEATURE_LABEL");
    unset_env_var("NESTED_VALUE_INT");
    unset_env_var("NESTED_VALUE_BOOL");
    unset_env_var("NESTED_VALUE_STRING");

    set_env_var("VIX_ENV_SILENT", "true");
  }

  static std::filesystem::path make_empty_env_path()
  {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("vix_config_typed_accessors_test_" + std::to_string(stamp));

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

  static Config make_config_from_current_env()
  {
    const std::filesystem::path env_path = make_empty_env_path();

    Config config{env_path};

    return config;
  }

  static void test_get_int_missing_key_returns_default()
  {
    Config config = make_config();

    assert(config.getInt("", 10) == 10);
    assert(config.getInt("missing", 20) == 20);
    assert(config.getInt("missing.value", 30) == 30);
    assert(config.getInt("server.missing.port", 40) == 40);
  }

  static void test_get_int_from_integer_values()
  {
    Config config = make_config();

    config.set("values.positive", 42);
    config.set("values.zero", 0);
    config.set("values.negative", -42);

    assert(config.getInt("values.positive", -1) == 42);
    assert(config.getInt("values.zero", -1) == 0);
    assert(config.getInt("values.negative", 0) == -42);
  }

  static void test_get_int_from_unsigned_values()
  {
    Config config = make_config();

    config.set("values.small", 7u);
    config.set("values.medium", 65535u);

    assert(config.getInt("values.small", -1) == 7);
    assert(config.getInt("values.medium", -1) == 65535);
  }

  static void test_get_int_from_float_values_truncates_toward_zero()
  {
    Config config = make_config();

    config.set("values.positive_float", 3.99);
    config.set("values.negative_float", -3.99);
    config.set("values.zero_float", 0.75);

    assert(config.getInt("values.positive_float", -1) == 3);
    assert(config.getInt("values.negative_float", 0) == -3);
    assert(config.getInt("values.zero_float", -1) == 0);
  }

  static void test_get_int_from_boolean_values()
  {
    Config config = make_config();

    config.set("flags.enabled", true);
    config.set("flags.disabled", false);

    assert(config.getInt("flags.enabled", 0) == 1);
    assert(config.getInt("flags.disabled", 1) == 0);
  }

  static void test_get_int_from_valid_string_values()
  {
    Config config = make_config();

    config.set("strings.positive", "123");
    config.set("strings.zero", "0");
    config.set("strings.negative", "-123");
    config.set("strings.plus", "+123");

    assert(config.getInt("strings.positive", -1) == 123);
    assert(config.getInt("strings.zero", -1) == 0);
    assert(config.getInt("strings.negative", 0) == -123);
    assert(config.getInt("strings.plus", -1) == 123);
  }

  static void test_get_int_from_invalid_string_values_returns_default()
  {
    Config config = make_config();

    config.set("strings.empty", "");
    config.set("strings.text", "abc");
    config.set("strings.partial_prefix", "12px");
    config.set("strings.partial_suffix", "px12");
    config.set("strings.float_text", "12.5");
    config.set("strings.trailing_space", "12 ");

    assert(config.getInt("strings.empty", 10) == 10);
    assert(config.getInt("strings.text", 20) == 20);
    assert(config.getInt("strings.partial_prefix", 30) == 30);
    assert(config.getInt("strings.partial_suffix", 40) == 40);
    assert(config.getInt("strings.float_text", 50) == 50);
    assert(config.getInt("strings.trailing_space", 60) == 60);
  }

  static void test_get_int_from_null_array_object_returns_default()
  {
    Config config = make_config();

    config.set("values.null_value", nullptr);
    config.set("values.array_value", Json::array({1, 2, 3}));
    config.set("values.object_value", Json::object({{"n", 1}}));

    assert(config.getInt("values.null_value", 10) == 10);
    assert(config.getInt("values.array_value", 20) == 20);
    assert(config.getInt("values.object_value", 30) == 30);
  }

  static void test_get_int_raw_value_has_priority_over_environment()
  {
    clear_config_env();

    set_env_var("APP_PORT", "3000");

    Config config = make_config_from_current_env();

    assert(config.getInt("app.port", -1) == 3000);

    config.set("app.port", 4000);

    assert(config.getInt("app.port", -1) == 4000);
  }

  static void test_get_int_environment_fallback()
  {
    clear_config_env();

    set_env_var("APP_INT", "77");
    set_env_var("FEATURE_COUNT", "-9");
    set_env_var("NESTED_VALUE_INT", "1234");

    Config config = make_config_from_current_env();

    assert(config.getInt("app.int", -1) == 77);
    assert(config.getInt("feature.count", 0) == -9);
    assert(config.getInt("nested.value.int", -1) == 1234);
  }

  static void test_get_int_invalid_environment_value_returns_default()
  {
    clear_config_env();

    set_env_var("APP_INT", "abc");
    set_env_var("FEATURE_COUNT", "10px");

    Config config = make_config_from_current_env();

    assert(config.has("app.int") == true);
    assert(config.has("feature.count") == true);

    assert(config.getInt("app.int", 10) == 10);
    assert(config.getInt("feature.count", 20) == 20);
  }

  static void test_get_bool_missing_key_returns_default()
  {
    Config config = make_config();

    assert(config.getBool("", true) == true);
    assert(config.getBool("", false) == false);

    assert(config.getBool("missing", true) == true);
    assert(config.getBool("missing", false) == false);

    assert(config.getBool("missing.value", true) == true);
    assert(config.getBool("missing.value", false) == false);
  }

  static void test_get_bool_from_boolean_values()
  {
    Config config = make_config();

    config.set("flags.enabled", true);
    config.set("flags.disabled", false);

    assert(config.getBool("flags.enabled", false) == true);
    assert(config.getBool("flags.disabled", true) == false);
  }

  static void test_get_bool_from_integer_values()
  {
    Config config = make_config();

    config.set("ints.zero", 0);
    config.set("ints.one", 1);
    config.set("ints.negative", -1);
    config.set("ints.large", 100);

    assert(config.getBool("ints.zero", true) == false);
    assert(config.getBool("ints.one", false) == true);
    assert(config.getBool("ints.negative", false) == true);
    assert(config.getBool("ints.large", false) == true);
  }

  static void test_get_bool_from_unsigned_values()
  {
    Config config = make_config();

    config.set("uints.zero", 0u);
    config.set("uints.one", 1u);
    config.set("uints.large", 100u);

    assert(config.getBool("uints.zero", true) == false);
    assert(config.getBool("uints.one", false) == true);
    assert(config.getBool("uints.large", false) == true);
  }

  static void test_get_bool_from_supported_string_values()
  {
    Config config = make_config();

    config.set("flags.true_lower", "true");
    config.set("flags.true_upper", "TRUE");
    config.set("flags.true_mixed", "TrUe");
    config.set("flags.true_one", "1");
    config.set("flags.true_yes", "yes");
    config.set("flags.true_on", "on");

    config.set("flags.false_lower", "false");
    config.set("flags.false_upper", "FALSE");
    config.set("flags.false_mixed", "FaLsE");
    config.set("flags.false_zero", "0");
    config.set("flags.false_no", "no");
    config.set("flags.false_off", "off");

    assert(config.getBool("flags.true_lower", false) == true);
    assert(config.getBool("flags.true_upper", false) == true);
    assert(config.getBool("flags.true_mixed", false) == true);
    assert(config.getBool("flags.true_one", false) == true);
    assert(config.getBool("flags.true_yes", false) == true);
    assert(config.getBool("flags.true_on", false) == true);

    assert(config.getBool("flags.false_lower", true) == false);
    assert(config.getBool("flags.false_upper", true) == false);
    assert(config.getBool("flags.false_mixed", true) == false);
    assert(config.getBool("flags.false_zero", true) == false);
    assert(config.getBool("flags.false_no", true) == false);
    assert(config.getBool("flags.false_off", true) == false);
  }

  static void test_get_bool_from_unsupported_string_values_returns_default()
  {
    Config config = make_config();

    config.set("flags.empty", "");
    config.set("flags.maybe", "maybe");
    config.set("flags.two", "2");
    config.set("flags.true_with_space", "true ");
    config.set("flags.false_with_space", " false");

    assert(config.getBool("flags.empty", true) == true);
    assert(config.getBool("flags.empty", false) == false);

    assert(config.getBool("flags.maybe", true) == true);
    assert(config.getBool("flags.maybe", false) == false);

    assert(config.getBool("flags.two", true) == true);
    assert(config.getBool("flags.two", false) == false);

    assert(config.getBool("flags.true_with_space", true) == true);
    assert(config.getBool("flags.true_with_space", false) == false);

    assert(config.getBool("flags.false_with_space", true) == true);
    assert(config.getBool("flags.false_with_space", false) == false);
  }

  static void test_get_bool_from_float_null_array_object_returns_default()
  {
    Config config = make_config();

    config.set("values.float_value", 1.0);
    config.set("values.null_value", nullptr);
    config.set("values.array_value", Json::array({true}));
    config.set("values.object_value", Json::object({{"enabled", true}}));

    assert(config.getBool("values.float_value", true) == true);
    assert(config.getBool("values.float_value", false) == false);

    assert(config.getBool("values.null_value", true) == true);
    assert(config.getBool("values.null_value", false) == false);

    assert(config.getBool("values.array_value", true) == true);
    assert(config.getBool("values.array_value", false) == false);

    assert(config.getBool("values.object_value", true) == true);
    assert(config.getBool("values.object_value", false) == false);
  }

  static void test_get_bool_raw_value_has_priority_over_environment()
  {
    clear_config_env();

    set_env_var("APP_DEBUG", "false");

    Config config = make_config_from_current_env();

    assert(config.getBool("app.debug", true) == false);

    config.set("app.debug", true);

    assert(config.getBool("app.debug", false) == true);
  }

  static void test_get_bool_environment_fallback()
  {
    clear_config_env();

    set_env_var("APP_BOOL", "true");
    set_env_var("FEATURE_ENABLED", "0");
    set_env_var("NESTED_VALUE_BOOL", "yes");

    Config config = make_config_from_current_env();

    assert(config.getBool("app.bool", false) == true);
    assert(config.getBool("feature.enabled", true) == false);
    assert(config.getBool("nested.value.bool", false) == true);
  }

  static void test_get_bool_invalid_environment_value_returns_default()
  {
    clear_config_env();

    set_env_var("APP_BOOL", "maybe");

    Config config = make_config_from_current_env();

    assert(config.has("app.bool") == true);

    assert(config.getBool("app.bool", true) == true);
    assert(config.getBool("app.bool", false) == false);
  }

  static void test_get_string_missing_key_returns_default()
  {
    Config config = make_config();

    assert(config.getString("", "fallback") == "fallback");
    assert(config.getString("missing", "fallback") == "fallback");
    assert(config.getString("missing.value", "fallback") == "fallback");
  }

  static void test_get_string_from_string_values()
  {
    Config config = make_config();

    config.set("strings.empty", "");
    config.set("strings.name", "Vix.cpp");
    config.set("strings.path", "/tmp/vix");
    config.set("strings.number_text", "123");

    assert(config.getString("strings.empty", "fallback") == "");
    assert(config.getString("strings.name", "fallback") == "Vix.cpp");
    assert(config.getString("strings.path", "fallback") == "/tmp/vix");
    assert(config.getString("strings.number_text", "fallback") == "123");
  }

  static void test_get_string_from_boolean_values()
  {
    Config config = make_config();

    config.set("flags.enabled", true);
    config.set("flags.disabled", false);

    assert(config.getString("flags.enabled", "missing") == "true");
    assert(config.getString("flags.disabled", "missing") == "false");
  }

  static void test_get_string_from_integer_values()
  {
    Config config = make_config();

    config.set("ints.positive", 42);
    config.set("ints.zero", 0);
    config.set("ints.negative", -42);

    assert(config.getString("ints.positive", "missing") == "42");
    assert(config.getString("ints.zero", "missing") == "0");
    assert(config.getString("ints.negative", "missing") == "-42");
  }

  static void test_get_string_from_unsigned_values()
  {
    Config config = make_config();

    config.set("uints.small", 42u);
    config.set("uints.zero", 0u);

    assert(config.getString("uints.small", "missing") == "42");
    assert(config.getString("uints.zero", "missing") == "0");
  }

  static void test_get_string_from_float_values()
  {
    Config config = make_config();

    config.set("floats.one_half", 1.5);
    config.set("floats.negative", -2.25);
    config.set("floats.zero", 0.0);

    const std::string one_half = config.getString("floats.one_half", "missing");
    const std::string negative = config.getString("floats.negative", "missing");
    const std::string zero = config.getString("floats.zero", "missing");

    assert(one_half.find("1.500000") == 0);
    assert(negative.find("-2.250000") == 0);
    assert(zero.find("0.000000") == 0);
  }

  static void test_get_string_from_null_array_object_returns_default()
  {
    Config config = make_config();

    config.set("values.null_value", nullptr);
    config.set("values.array_value", Json::array({"a", "b"}));
    config.set("values.object_value", Json::object({{"name", "vix"}}));

    assert(config.getString("values.null_value", "fallback") == "fallback");
    assert(config.getString("values.array_value", "fallback") == "fallback");
    assert(config.getString("values.object_value", "fallback") == "fallback");
  }

  static void test_get_string_raw_value_has_priority_over_environment()
  {
    clear_config_env();

    set_env_var("APP_NAME", "env-name");

    Config config = make_config_from_current_env();

    assert(config.getString("app.name", "missing") == "env-name");

    config.set("app.name", "raw-name");

    assert(config.getString("app.name", "missing") == "raw-name");
  }

  static void test_get_string_environment_fallback()
  {
    clear_config_env();

    set_env_var("APP_STRING", "hello");
    set_env_var("FEATURE_LABEL", "feature-a");
    set_env_var("NESTED_VALUE_STRING", "nested");

    Config config = make_config_from_current_env();

    assert(config.getString("app.string", "missing") == "hello");
    assert(config.getString("feature.label", "missing") == "feature-a");
    assert(config.getString("nested.value.string", "missing") == "nested");
  }

  static void test_get_string_environment_empty_value_is_preserved()
  {
    clear_config_env();

    set_env_var("APP_STRING", "");

    Config config = make_config_from_current_env();

    assert(config.has("app.string") == true);
    assert(config.getString("app.string", "fallback") == "");
  }

  static void test_has_prefers_raw_config_but_can_see_environment()
  {
    clear_config_env();

    set_env_var("APP_NAME", "env-name");

    Config config = make_config_from_current_env();

    assert(config.has("app.name") == true);
    assert(config.getString("app.name", "missing") == "env-name");

    config.set("app.name", "raw-name");

    assert(config.has("app.name") == true);
    assert(config.getString("app.name", "missing") == "raw-name");
  }

  static void test_parent_object_created_by_set_is_present_but_not_convertible()
  {
    Config config = make_config();

    config.set("server.http.port", 8080);

    assert(config.has("server") == true);
    assert(config.has("server.http") == true);
    assert(config.has("server.http.port") == true);

    assert(config.getInt("server", 10) == 10);
    assert(config.getBool("server", false) == false);
    assert(config.getString("server", "fallback") == "fallback");

    assert(config.getInt("server.http", 20) == 20);
    assert(config.getBool("server.http", true) == true);
    assert(config.getString("server.http", "fallback") == "fallback");
  }

  static void test_overwriting_value_changes_accessor_behavior()
  {
    Config config = make_config();

    config.set("value", "42");

    assert(config.getString("value", "missing") == "42");
    assert(config.getInt("value", -1) == 42);
    assert(config.getBool("value", false) == false);

    config.set("value", true);

    assert(config.getString("value", "missing") == "true");
    assert(config.getInt("value", -1) == 1);
    assert(config.getBool("value", false) == true);

    config.set("value", Json::array({1, 2, 3}));

    assert(config.has("value") == true);
    assert(config.getString("value", "fallback") == "fallback");
    assert(config.getInt("value", 123) == 123);
    assert(config.getBool("value", false) == false);
  }

} // namespace

int main()
{
  test_get_int_missing_key_returns_default();
  test_get_int_from_integer_values();
  test_get_int_from_unsigned_values();
  test_get_int_from_float_values_truncates_toward_zero();
  test_get_int_from_boolean_values();
  test_get_int_from_valid_string_values();
  test_get_int_from_invalid_string_values_returns_default();
  test_get_int_from_null_array_object_returns_default();
  test_get_int_raw_value_has_priority_over_environment();
  test_get_int_environment_fallback();
  test_get_int_invalid_environment_value_returns_default();

  test_get_bool_missing_key_returns_default();
  test_get_bool_from_boolean_values();
  test_get_bool_from_integer_values();
  test_get_bool_from_unsigned_values();
  test_get_bool_from_supported_string_values();
  test_get_bool_from_unsupported_string_values_returns_default();
  test_get_bool_from_float_null_array_object_returns_default();
  test_get_bool_raw_value_has_priority_over_environment();
  test_get_bool_environment_fallback();
  test_get_bool_invalid_environment_value_returns_default();

  test_get_string_missing_key_returns_default();
  test_get_string_from_string_values();
  test_get_string_from_boolean_values();
  test_get_string_from_integer_values();
  test_get_string_from_unsigned_values();
  test_get_string_from_float_values();
  test_get_string_from_null_array_object_returns_default();
  test_get_string_raw_value_has_priority_over_environment();
  test_get_string_environment_fallback();
  test_get_string_environment_empty_value_is_preserved();

  test_has_prefers_raw_config_but_can_see_environment();
  test_parent_object_created_by_set_is_present_but_not_convertible();
  test_overwriting_value_changes_accessor_behavior();

  return 0;
}
