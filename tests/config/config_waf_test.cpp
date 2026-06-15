/**
 *
 * @file config_waf_test.cpp
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

namespace
{
  using Config = vix::config::Config;

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
        ("vix_config_waf_test_" + std::to_string(stamp));

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

  static void test_default_waf_values()
  {
    Config config = make_clean_config();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_mode_off_from_environment()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "off");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "off");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_mode_basic_from_environment()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "basic");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "basic");
  }

  static void test_waf_mode_strict_from_environment()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "strict");
  }

  static void test_waf_mode_custom_value_is_preserved()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "custom");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "custom");
  }

  static void test_waf_mode_empty_value_is_preserved()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "");
  }

  static void test_waf_target_limit_from_environment()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "2048");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 2048);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_body_limit_from_environment()
  {
    clear_config_env();

    set_env_var("WAF_MAX_BODY_BYTES", "65536");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 65536);
  }

  static void test_all_waf_environment_values()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "8192");
    set_env_var("WAF_MAX_BODY_BYTES", "2097152");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 8192);
    assert(config.getWafMaxBodyBytes() == 2097152);
  }

  static void test_waf_target_limit_zero_is_preserved()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "0");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxTargetLen() == 0);
  }

  static void test_waf_body_limit_zero_is_preserved()
  {
    clear_config_env();

    set_env_var("WAF_MAX_BODY_BYTES", "0");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxBodyBytes() == 0);
  }

  static void test_waf_target_limit_negative_is_preserved()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "-1");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxTargetLen() == -1);
  }

  static void test_waf_body_limit_negative_is_preserved()
  {
    clear_config_env();

    set_env_var("WAF_MAX_BODY_BYTES", "-1");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxBodyBytes() == -1);
  }

  static void test_waf_target_limit_invalid_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "large");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxTargetLen() == 4096);
  }

  static void test_waf_body_limit_invalid_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("WAF_MAX_BODY_BYTES", "huge");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_target_limit_partial_number_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "4096px");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxTargetLen() == 4096);
  }

  static void test_waf_body_limit_partial_number_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("WAF_MAX_BODY_BYTES", "1048576bytes");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_empty_int_env_values_fall_back_to_defaults()
  {
    clear_config_env();

    set_env_var("WAF_MAX_TARGET_LEN", "");
    set_env_var("WAF_MAX_BODY_BYTES", "");

    Config config = make_config_from_current_env();

    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_environment_keys_are_visible_through_dotted_accessors()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "1234");
    set_env_var("WAF_MAX_BODY_BYTES", "5678");

    Config config = make_config_from_current_env();

    assert(config.has("waf.mode") == true);
    assert(config.has("waf.max.target.len") == true);
    assert(config.has("waf.max.body.bytes") == true);

    assert(config.getString("waf.mode", "missing") == "strict");
    assert(config.getInt("waf.max.target.len", -1) == 1234);
    assert(config.getInt("waf.max.body.bytes", -1) == 5678);

    assert(config.getString("waf.max.target.len", "missing") == "1234");
    assert(config.getString("waf.max.body.bytes", "missing") == "5678");
  }

  static void test_waf_missing_dotted_keys_return_fallbacks()
  {
    Config config = make_clean_config();

    assert(config.has("waf.mode") == false);
    assert(config.has("waf.max.target.len") == false);
    assert(config.has("waf.max.body.bytes") == false);

    assert(config.getString("waf.mode", "fallback") == "fallback");
    assert(config.getInt("waf.max.target.len", 111) == 111);
    assert(config.getInt("waf.max.body.bytes", 222) == 222);
  }

  static void test_raw_waf_keys_do_not_change_dedicated_waf_accessors()
  {
    Config config = make_clean_config();

    config.set("waf.mode", "strict");
    config.set("waf.max.target.len", 1234);
    config.set("waf.max.body.bytes", 5678);

    assert(config.has("waf.mode") == true);
    assert(config.has("waf.max.target.len") == true);
    assert(config.has("waf.max.body.bytes") == true);

    assert(config.getString("waf.mode", "missing") == "strict");
    assert(config.getInt("waf.max.target.len", -1) == 1234);
    assert(config.getInt("waf.max.body.bytes", -1) == 5678);

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_raw_waf_keys_override_environment_for_dotted_accessors_only()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "basic");
    set_env_var("WAF_MAX_TARGET_LEN", "1111");
    set_env_var("WAF_MAX_BODY_BYTES", "2222");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 1111);
    assert(config.getWafMaxBodyBytes() == 2222);

    assert(config.getString("waf.mode", "missing") == "basic");
    assert(config.getInt("waf.max.target.len", -1) == 1111);
    assert(config.getInt("waf.max.body.bytes", -1) == 2222);

    config.set("waf.mode", "strict");
    config.set("waf.max.target.len", 3333);
    config.set("waf.max.body.bytes", 4444);

    assert(config.getString("waf.mode", "missing") == "strict");
    assert(config.getInt("waf.max.target.len", -1) == 3333);
    assert(config.getInt("waf.max.body.bytes", -1) == 4444);

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 1111);
    assert(config.getWafMaxBodyBytes() == 2222);
  }

  static void test_load_config_updates_waf_values_from_environment()
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

  static void test_load_config_resets_waf_values_to_defaults_when_env_removed()
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

  static void test_load_config_clears_raw_waf_values()
  {
    Config config = make_clean_config();

    config.set("waf.mode", "strict");
    config.set("waf.max.target.len", 1234);
    config.set("waf.max.body.bytes", 5678);

    assert(config.has("waf.mode") == true);
    assert(config.getString("waf.mode", "missing") == "strict");
    assert(config.getInt("waf.max.target.len", -1) == 1234);
    assert(config.getInt("waf.max.body.bytes", -1) == 5678);

    config.loadConfig();

    assert(config.has("waf.mode") == false);
    assert(config.has("waf.max.target.len") == false);
    assert(config.has("waf.max.body.bytes") == false);

    assert(config.getString("waf.mode", "fallback") == "fallback");
    assert(config.getInt("waf.max.target.len", 111) == 111);
    assert(config.getInt("waf.max.body.bytes", 222) == 222);

    assert(config.getWafMode() == "basic");
    assert(config.getWafMaxTargetLen() == 4096);
    assert(config.getWafMaxBodyBytes() == 1024 * 1024);
  }

  static void test_waf_values_are_independent_from_server_values()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "1234");
    set_env_var("WAF_MAX_BODY_BYTES", "5678");
    set_env_var("SERVER_PORT", "18080");

    Config config = make_config_from_current_env();

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 1234);
    assert(config.getWafMaxBodyBytes() == 5678);

    assert(config.getServerPort() == 18080);

    config.setServerPort(19090);

    assert(config.getServerPort() == 19090);

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 1234);
    assert(config.getWafMaxBodyBytes() == 5678);
  }

  static void test_copy_preserves_waf_values()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "7000");
    set_env_var("WAF_MAX_BODY_BYTES", "8000");

    Config original = make_config_from_current_env();
    Config copy = original;

    assert(copy.getWafMode() == "strict");
    assert(copy.getWafMaxTargetLen() == 7000);
    assert(copy.getWafMaxBodyBytes() == 8000);

    assert(original.getWafMode() == "strict");
    assert(original.getWafMaxTargetLen() == 7000);
    assert(original.getWafMaxBodyBytes() == 8000);
  }

  static void test_copy_assignment_preserves_waf_values()
  {
    clear_config_env();

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "9000");
    set_env_var("WAF_MAX_BODY_BYTES", "10000");

    Config source = make_config_from_current_env();

    clear_config_env();

    Config target = make_config_from_current_env();

    assert(target.getWafMode() == "basic");
    assert(target.getWafMaxTargetLen() == 4096);
    assert(target.getWafMaxBodyBytes() == 1024 * 1024);

    target = source;

    assert(target.getWafMode() == "strict");
    assert(target.getWafMaxTargetLen() == 9000);
    assert(target.getWafMaxBodyBytes() == 10000);
  }

} // namespace

int main()
{
  test_default_waf_values();

  test_waf_mode_off_from_environment();
  test_waf_mode_basic_from_environment();
  test_waf_mode_strict_from_environment();
  test_waf_mode_custom_value_is_preserved();
  test_waf_mode_empty_value_is_preserved();

  test_waf_target_limit_from_environment();
  test_waf_body_limit_from_environment();
  test_all_waf_environment_values();

  test_waf_target_limit_zero_is_preserved();
  test_waf_body_limit_zero_is_preserved();
  test_waf_target_limit_negative_is_preserved();
  test_waf_body_limit_negative_is_preserved();

  test_waf_target_limit_invalid_value_falls_back_to_default();
  test_waf_body_limit_invalid_value_falls_back_to_default();
  test_waf_target_limit_partial_number_falls_back_to_default();
  test_waf_body_limit_partial_number_falls_back_to_default();
  test_waf_empty_int_env_values_fall_back_to_defaults();

  test_waf_environment_keys_are_visible_through_dotted_accessors();
  test_waf_missing_dotted_keys_return_fallbacks();

  test_raw_waf_keys_do_not_change_dedicated_waf_accessors();
  test_raw_waf_keys_override_environment_for_dotted_accessors_only();

  test_load_config_updates_waf_values_from_environment();
  test_load_config_resets_waf_values_to_defaults_when_env_removed();
  test_load_config_clears_raw_waf_values();

  test_waf_values_are_independent_from_server_values();

  test_copy_preserves_waf_values();
  test_copy_assignment_preserves_waf_values();

  return 0;
}
