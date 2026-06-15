/**
 *
 * @file config_database_test.cpp
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
        ("vix_config_database_test_" + std::to_string(stamp));

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

  static void test_default_database_values()
  {
    Config config = make_clean_config();

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
    assert(config.getDbPasswordFromEnv() == "");
  }

  static void test_database_host_from_environment()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "db.internal");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "db.internal");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
  }

  static void test_database_user_from_environment()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_USER", "vix_user");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "vix_user");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
  }

  static void test_database_name_from_environment()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_NAME", "vix_database");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "vix_database");
    assert(config.getDbPort() == 3306);
  }

  static void test_database_port_from_environment()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "5432");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == 5432);
  }

  static void test_all_database_environment_values()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "postgres.internal");
    set_env_var("DATABASE_DEFAULT_USER", "service_user");
    set_env_var("DATABASE_DEFAULT_NAME", "service_db");
    set_env_var("DATABASE_DEFAULT_PORT", "5432");
    set_env_var("DATABASE_DEFAULT_PASSWORD", "service_secret");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "postgres.internal");
    assert(config.getDbUser() == "service_user");
    assert(config.getDbName() == "service_db");
    assert(config.getDbPort() == 5432);
    assert(config.getDbPasswordFromEnv() == "service_secret");
  }

  static void test_database_empty_environment_values_are_preserved_for_strings()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "");
    set_env_var("DATABASE_DEFAULT_USER", "");
    set_env_var("DATABASE_DEFAULT_NAME", "");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "");
    assert(config.getDbUser() == "");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
  }

  static void test_database_port_zero_is_preserved()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "0");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == 0);
  }

  static void test_database_port_negative_is_preserved()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "-1");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == -1);
  }

  static void test_database_port_invalid_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "not-a-port");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == 3306);
  }

  static void test_database_port_partial_number_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "5432abc");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == 3306);
  }

  static void test_database_port_empty_value_falls_back_to_default()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PORT", "");

    Config config = make_config_from_current_env();

    assert(config.getDbPort() == 3306);
  }

  static void test_database_password_default_is_empty()
  {
    Config config = make_clean_config();

    assert(config.getDbPasswordFromEnv() == "");
  }

  static void test_database_password_from_database_default_password()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PASSWORD", "default-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "default-password");
  }

  static void test_database_password_from_vix_db_password()
  {
    clear_config_env();

    set_env_var("VIX_DB_PASSWORD", "vix-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "vix-password");
  }

  static void test_database_password_from_db_password()
  {
    clear_config_env();

    set_env_var("DB_PASSWORD", "db-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "db-password");
  }

  static void test_database_password_from_mysql_password()
  {
    clear_config_env();

    set_env_var("MYSQL_PASSWORD", "mysql-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "mysql-password");
  }

  static void test_database_password_priority_prefers_vix_db_password()
  {
    clear_config_env();

    set_env_var("VIX_DB_PASSWORD", "vix-password");
    set_env_var("DATABASE_DEFAULT_PASSWORD", "default-password");
    set_env_var("DB_PASSWORD", "db-password");
    set_env_var("MYSQL_PASSWORD", "mysql-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "vix-password");
  }

  static void test_database_password_priority_prefers_database_default_over_db_password()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PASSWORD", "default-password");
    set_env_var("DB_PASSWORD", "db-password");
    set_env_var("MYSQL_PASSWORD", "mysql-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "default-password");
  }

  static void test_database_password_priority_prefers_db_password_over_mysql_password()
  {
    clear_config_env();

    set_env_var("DB_PASSWORD", "db-password");
    set_env_var("MYSQL_PASSWORD", "mysql-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "db-password");
  }

  static void test_database_password_empty_high_priority_value_is_ignored()
  {
    clear_config_env();

    set_env_var("VIX_DB_PASSWORD", "");
    set_env_var("DATABASE_DEFAULT_PASSWORD", "default-password");
    set_env_var("DB_PASSWORD", "db-password");
    set_env_var("MYSQL_PASSWORD", "mysql-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "default-password");
  }

  static void test_database_password_is_read_from_current_environment()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_PASSWORD", "first-password");

    Config config = make_config_from_current_env();

    assert(config.getDbPasswordFromEnv() == "first-password");

    set_env_var("DATABASE_DEFAULT_PASSWORD", "second-password");

    assert(config.getDbPasswordFromEnv() == "second-password");
  }

  static void test_load_config_updates_database_values_from_environment()
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

  static void test_load_config_resets_database_values_to_defaults_when_env_removed()
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

  static void test_set_raw_database_keys_does_not_change_dedicated_database_accessors()
  {
    Config config = make_clean_config();

    config.set("database.default.host", "raw-host");
    config.set("database.default.user", "raw-user");
    config.set("database.default.name", "raw-name");
    config.set("database.default.port", 9999);

    assert(config.has("database.default.host") == true);
    assert(config.has("database.default.user") == true);
    assert(config.has("database.default.name") == true);
    assert(config.has("database.default.port") == true);

    assert(config.getString("database.default.host", "") == "raw-host");
    assert(config.getString("database.default.user", "") == "raw-user");
    assert(config.getString("database.default.name", "") == "raw-name");
    assert(config.getInt("database.default.port", -1) == 9999);

    assert(config.getDbHost() == "localhost");
    assert(config.getDbUser() == "root");
    assert(config.getDbName() == "");
    assert(config.getDbPort() == 3306);
  }

  static void test_environment_database_keys_are_visible_through_dotted_accessors()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "db.internal");
    set_env_var("DATABASE_DEFAULT_USER", "db_user");
    set_env_var("DATABASE_DEFAULT_NAME", "db_name");
    set_env_var("DATABASE_DEFAULT_PORT", "4567");

    Config config = make_config_from_current_env();

    assert(config.has("database.default.host") == true);
    assert(config.has("database.default.user") == true);
    assert(config.has("database.default.name") == true);
    assert(config.has("database.default.port") == true);

    assert(config.getString("database.default.host", "") == "db.internal");
    assert(config.getString("database.default.user", "") == "db_user");
    assert(config.getString("database.default.name", "") == "db_name");
    assert(config.getInt("database.default.port", -1) == 4567);
    assert(config.getString("database.default.port", "") == "4567");
  }

  static void test_raw_database_keys_override_environment_for_dotted_accessors_only()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "env-host");
    set_env_var("DATABASE_DEFAULT_USER", "env-user");
    set_env_var("DATABASE_DEFAULT_NAME", "env-name");
    set_env_var("DATABASE_DEFAULT_PORT", "1111");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "env-host");
    assert(config.getDbUser() == "env-user");
    assert(config.getDbName() == "env-name");
    assert(config.getDbPort() == 1111);

    config.set("database.default.host", "raw-host");
    config.set("database.default.user", "raw-user");
    config.set("database.default.name", "raw-name");
    config.set("database.default.port", 2222);

    assert(config.getString("database.default.host", "") == "raw-host");
    assert(config.getString("database.default.user", "") == "raw-user");
    assert(config.getString("database.default.name", "") == "raw-name");
    assert(config.getInt("database.default.port", -1) == 2222);

    assert(config.getDbHost() == "env-host");
    assert(config.getDbUser() == "env-user");
    assert(config.getDbName() == "env-name");
    assert(config.getDbPort() == 1111);
  }

  static void test_database_values_are_independent_from_server_values()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "db.internal");
    set_env_var("DATABASE_DEFAULT_PORT", "5432");
    set_env_var("SERVER_PORT", "18080");

    Config config = make_config_from_current_env();

    assert(config.getDbHost() == "db.internal");
    assert(config.getDbPort() == 5432);
    assert(config.getServerPort() == 18080);

    config.setServerPort(19090);

    assert(config.getServerPort() == 19090);
    assert(config.getDbHost() == "db.internal");
    assert(config.getDbPort() == 5432);
  }

  static void test_copy_preserves_database_values()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "copy-host");
    set_env_var("DATABASE_DEFAULT_USER", "copy-user");
    set_env_var("DATABASE_DEFAULT_NAME", "copy-name");
    set_env_var("DATABASE_DEFAULT_PORT", "7777");

    Config original = make_config_from_current_env();
    Config copy = original;

    assert(copy.getDbHost() == "copy-host");
    assert(copy.getDbUser() == "copy-user");
    assert(copy.getDbName() == "copy-name");
    assert(copy.getDbPort() == 7777);

    assert(original.getDbHost() == "copy-host");
    assert(original.getDbUser() == "copy-user");
    assert(original.getDbName() == "copy-name");
    assert(original.getDbPort() == 7777);
  }

  static void test_copy_assignment_preserves_database_values()
  {
    clear_config_env();

    set_env_var("DATABASE_DEFAULT_HOST", "source-host");
    set_env_var("DATABASE_DEFAULT_USER", "source-user");
    set_env_var("DATABASE_DEFAULT_NAME", "source-name");
    set_env_var("DATABASE_DEFAULT_PORT", "8888");

    Config source = make_config_from_current_env();

    clear_config_env();

    Config target = make_config_from_current_env();

    assert(target.getDbHost() == "localhost");
    assert(target.getDbUser() == "root");
    assert(target.getDbName() == "");
    assert(target.getDbPort() == 3306);

    target = source;

    assert(target.getDbHost() == "source-host");
    assert(target.getDbUser() == "source-user");
    assert(target.getDbName() == "source-name");
    assert(target.getDbPort() == 8888);
  }

} // namespace

int main()
{
  test_default_database_values();

  test_database_host_from_environment();
  test_database_user_from_environment();
  test_database_name_from_environment();
  test_database_port_from_environment();
  test_all_database_environment_values();

  test_database_empty_environment_values_are_preserved_for_strings();

  test_database_port_zero_is_preserved();
  test_database_port_negative_is_preserved();
  test_database_port_invalid_value_falls_back_to_default();
  test_database_port_partial_number_falls_back_to_default();
  test_database_port_empty_value_falls_back_to_default();

  test_database_password_default_is_empty();
  test_database_password_from_database_default_password();
  test_database_password_from_vix_db_password();
  test_database_password_from_db_password();
  test_database_password_from_mysql_password();

  test_database_password_priority_prefers_vix_db_password();
  test_database_password_priority_prefers_database_default_over_db_password();
  test_database_password_priority_prefers_db_password_over_mysql_password();
  test_database_password_empty_high_priority_value_is_ignored();
  test_database_password_is_read_from_current_environment();

  test_load_config_updates_database_values_from_environment();
  test_load_config_resets_database_values_to_defaults_when_env_removed();

  test_set_raw_database_keys_does_not_change_dedicated_database_accessors();
  test_environment_database_keys_are_visible_through_dotted_accessors();
  test_raw_database_keys_override_environment_for_dotted_accessors_only();

  test_database_values_are_independent_from_server_values();

  test_copy_preserves_database_values();
  test_copy_assignment_preserves_database_values();

  return 0;
}
