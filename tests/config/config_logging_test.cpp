/**
 *
 * @file config_logging_test.cpp
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
        ("vix_config_logging_test_" + std::to_string(stamp));

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

  static void test_default_logging_values()
  {
    Config config = make_clean_config();

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_logging_async_true_values()
  {
    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "true");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == true);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "1");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == true);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "yes");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == true);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "on");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == true);
    }
  }

  static void test_logging_async_false_values()
  {
    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "false");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == false);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "0");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == false);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "no");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == false);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_ASYNC", "off");

      Config config = make_config_from_current_env();

      assert(config.getLogAsync() == false);
    }
  }

  static void test_logging_async_invalid_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "maybe");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == true);
  }

  static void test_logging_async_empty_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == true);
  }

  static void test_logging_drop_on_overflow_true_values()
  {
    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "true");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == true);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "1");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == true);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "yes");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == true);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "on");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == true);
    }
  }

  static void test_logging_drop_on_overflow_false_values()
  {
    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == false);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "0");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == false);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "no");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == false);
    }

    {
      clear_config_env();

      set_env_var("LOGGING_DROP_ON_OVERFLOW", "off");

      Config config = make_config_from_current_env();

      assert(config.getLogDropOnOverflow() == false);
    }
  }

  static void test_logging_drop_on_overflow_invalid_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_DROP_ON_OVERFLOW", "maybe");

    Config config = make_config_from_current_env();

    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_logging_drop_on_overflow_empty_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_DROP_ON_OVERFLOW", "");

    Config config = make_config_from_current_env();

    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_logging_queue_max_from_environment()
  {
    clear_config_env();

    set_env_var("LOGGING_QUEUE_MAX", "1000");

    Config config = make_config_from_current_env();

    assert(config.getLogQueueMax() == 1000);
  }

  static void test_logging_queue_max_zero_is_preserved()
  {
    clear_config_env();

    set_env_var("LOGGING_QUEUE_MAX", "0");

    Config config = make_config_from_current_env();

    assert(config.getLogQueueMax() == 0);
  }

  static void test_logging_queue_max_negative_is_preserved()
  {
    clear_config_env();

    set_env_var("LOGGING_QUEUE_MAX", "-1");

    Config config = make_config_from_current_env();

    assert(config.getLogQueueMax() == -1);
  }

  static void test_logging_queue_max_invalid_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_QUEUE_MAX", "large");

    Config config = make_config_from_current_env();

    assert(config.getLogQueueMax() == 20000);
  }

  static void test_logging_queue_max_partial_number_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_QUEUE_MAX", "1000items");

    Config config = make_config_from_current_env();

    assert(config.getLogQueueMax() == 20000);
  }

  static void test_logging_queue_max_empty_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("LOGGING_QUEUE_MAX", "");

    Config config = make_config_from_current_env();

    assert(config.getLogQueueMax() == 20000);
  }

  static void test_all_logging_environment_values()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "4096");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 4096);
    assert(config.getLogDropOnOverflow() == false);
  }

  static void test_logging_environment_keys_are_visible_through_dotted_accessors()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "1234");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "true");

    Config config = make_config_from_current_env();

    assert(config.has("logging.async") == true);
    assert(config.has("logging.queue.max") == true);
    assert(config.has("logging.drop.on.overflow") == true);

    assert(config.getBool("logging.async", true) == false);
    assert(config.getString("logging.async", "missing") == "false");

    assert(config.getInt("logging.queue.max", -1) == 1234);
    assert(config.getString("logging.queue.max", "missing") == "1234");

    assert(config.getBool("logging.drop.on.overflow", false) == true);
    assert(config.getString("logging.drop.on.overflow", "missing") == "true");
  }

  static void test_logging_missing_dotted_keys_return_fallbacks()
  {
    Config config = make_clean_config();

    assert(config.has("logging.async") == false);
    assert(config.has("logging.queue.max") == false);
    assert(config.has("logging.drop.on.overflow") == false);

    assert(config.getBool("logging.async", false) == false);
    assert(config.getBool("logging.async", true) == true);

    assert(config.getInt("logging.queue.max", 111) == 111);

    assert(config.getBool("logging.drop.on.overflow", false) == false);
    assert(config.getBool("logging.drop.on.overflow", true) == true);
  }

  static void test_raw_logging_keys_do_not_change_dedicated_logging_accessors()
  {
    Config config = make_clean_config();

    config.set("logging.async", false);
    config.set("logging.queue.max", 1234);
    config.set("logging.drop.on.overflow", false);

    assert(config.has("logging.async") == true);
    assert(config.has("logging.queue.max") == true);
    assert(config.has("logging.drop.on.overflow") == true);

    assert(config.getBool("logging.async", true) == false);
    assert(config.getInt("logging.queue.max", -1) == 1234);
    assert(config.getBool("logging.drop.on.overflow", true) == false);

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_raw_logging_keys_override_environment_for_dotted_accessors_only()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "true");
    set_env_var("LOGGING_QUEUE_MAX", "1111");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "true");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 1111);
    assert(config.getLogDropOnOverflow() == true);

    assert(config.getBool("logging.async", false) == true);
    assert(config.getInt("logging.queue.max", -1) == 1111);
    assert(config.getBool("logging.drop.on.overflow", false) == true);

    config.set("logging.async", false);
    config.set("logging.queue.max", 2222);
    config.set("logging.drop.on.overflow", false);

    assert(config.getBool("logging.async", true) == false);
    assert(config.getInt("logging.queue.max", -1) == 2222);
    assert(config.getBool("logging.drop.on.overflow", true) == false);

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 1111);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_load_config_updates_logging_values_from_environment()
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

  static void test_load_config_resets_logging_values_to_defaults_when_env_removed()
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

  static void test_load_config_clears_raw_logging_values()
  {
    Config config = make_clean_config();

    config.set("logging.async", false);
    config.set("logging.queue.max", 1234);
    config.set("logging.drop.on.overflow", false);

    assert(config.has("logging.async") == true);
    assert(config.has("logging.queue.max") == true);
    assert(config.has("logging.drop.on.overflow") == true);

    assert(config.getBool("logging.async", true) == false);
    assert(config.getInt("logging.queue.max", -1) == 1234);
    assert(config.getBool("logging.drop.on.overflow", true) == false);

    config.loadConfig();

    assert(config.has("logging.async") == false);
    assert(config.has("logging.queue.max") == false);
    assert(config.has("logging.drop.on.overflow") == false);

    assert(config.getBool("logging.async", true) == true);
    assert(config.getBool("logging.async", false) == false);
    assert(config.getInt("logging.queue.max", 222) == 222);
    assert(config.getBool("logging.drop.on.overflow", true) == true);
    assert(config.getBool("logging.drop.on.overflow", false) == false);

    assert(config.getLogAsync() == true);
    assert(config.getLogQueueMax() == 20000);
    assert(config.getLogDropOnOverflow() == true);
  }

  static void test_logging_values_are_independent_from_server_values()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "5000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");
    set_env_var("SERVER_PORT", "18080");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 5000);
    assert(config.getLogDropOnOverflow() == false);

    assert(config.getServerPort() == 18080);

    config.setServerPort(19090);

    assert(config.getServerPort() == 19090);

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 5000);
    assert(config.getLogDropOnOverflow() == false);
  }

  static void test_logging_values_are_independent_from_waf_values()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "6000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    set_env_var("WAF_MODE", "strict");
    set_env_var("WAF_MAX_TARGET_LEN", "1234");
    set_env_var("WAF_MAX_BODY_BYTES", "5678");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 6000);
    assert(config.getLogDropOnOverflow() == false);

    assert(config.getWafMode() == "strict");
    assert(config.getWafMaxTargetLen() == 1234);
    assert(config.getWafMaxBodyBytes() == 5678);
  }

  static void test_logging_values_are_independent_from_tls_values()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "7000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    set_env_var("SERVER_TLS_ENABLED", "true");
    set_env_var("SERVER_TLS_CERT_FILE", "cert.pem");
    set_env_var("SERVER_TLS_KEY_FILE", "key.pem");

    Config config = make_config_from_current_env();

    assert(config.getLogAsync() == false);
    assert(config.getLogQueueMax() == 7000);
    assert(config.getLogDropOnOverflow() == false);

    assert(config.isTlsEnabled() == true);
    assert(config.getTlsCertFile() == "cert.pem");
    assert(config.getTlsKeyFile() == "key.pem");
  }

  static void test_copy_preserves_logging_values()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "8000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    Config original = make_config_from_current_env();
    Config copy = original;

    assert(copy.getLogAsync() == false);
    assert(copy.getLogQueueMax() == 8000);
    assert(copy.getLogDropOnOverflow() == false);

    assert(original.getLogAsync() == false);
    assert(original.getLogQueueMax() == 8000);
    assert(original.getLogDropOnOverflow() == false);
  }

  static void test_copy_assignment_preserves_logging_values()
  {
    clear_config_env();

    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("LOGGING_QUEUE_MAX", "9000");
    set_env_var("LOGGING_DROP_ON_OVERFLOW", "false");

    Config source = make_config_from_current_env();

    clear_config_env();

    Config target = make_config_from_current_env();

    assert(target.getLogAsync() == true);
    assert(target.getLogQueueMax() == 20000);
    assert(target.getLogDropOnOverflow() == true);

    target = source;

    assert(target.getLogAsync() == false);
    assert(target.getLogQueueMax() == 9000);
    assert(target.getLogDropOnOverflow() == false);
  }

} // namespace

int main()
{
  test_default_logging_values();

  test_logging_async_true_values();
  test_logging_async_false_values();
  test_logging_async_invalid_value_falls_back_to_default();
  test_logging_async_empty_value_falls_back_to_default();

  test_logging_drop_on_overflow_true_values();
  test_logging_drop_on_overflow_false_values();
  test_logging_drop_on_overflow_invalid_value_falls_back_to_default();
  test_logging_drop_on_overflow_empty_value_falls_back_to_default();

  test_logging_queue_max_from_environment();
  test_logging_queue_max_zero_is_preserved();
  test_logging_queue_max_negative_is_preserved();
  test_logging_queue_max_invalid_value_falls_back_to_default();
  test_logging_queue_max_partial_number_falls_back_to_default();
  test_logging_queue_max_empty_value_falls_back_to_default();

  test_all_logging_environment_values();

  test_logging_environment_keys_are_visible_through_dotted_accessors();
  test_logging_missing_dotted_keys_return_fallbacks();

  test_raw_logging_keys_do_not_change_dedicated_logging_accessors();
  test_raw_logging_keys_override_environment_for_dotted_accessors_only();

  test_load_config_updates_logging_values_from_environment();
  test_load_config_resets_logging_values_to_defaults_when_env_removed();
  test_load_config_clears_raw_logging_values();

  test_logging_values_are_independent_from_server_values();
  test_logging_values_are_independent_from_waf_values();
  test_logging_values_are_independent_from_tls_values();

  test_copy_preserves_logging_values();
  test_copy_assignment_preserves_logging_values();

  return 0;
}
