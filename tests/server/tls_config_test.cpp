/**
 *
 * @file tls_config_test.cpp
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
#include <string>
#include <type_traits>
#include <utility>

#include <vix/server/TlsConfig.hpp>

namespace
{
  using TlsConfig = vix::server::TlsConfig;

  static void test_default_tls_config_is_disabled_and_invalid()
  {
    TlsConfig config;

    assert(config.enabled == false);
    assert(config.cert_file.empty());
    assert(config.key_file.empty());

    assert(config.is_enabled() == false);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_enabled_without_files_is_not_configured_and_invalid()
  {
    TlsConfig config;

    config.enabled = true;

    assert(config.is_enabled() == true);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_cert_file_without_key_file_is_not_configured()
  {
    TlsConfig config;

    config.cert_file = "/etc/letsencrypt/live/example.com/fullchain.pem";

    assert(config.enabled == false);
    assert(config.is_enabled() == false);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_key_file_without_cert_file_is_not_configured()
  {
    TlsConfig config;

    config.key_file = "/etc/letsencrypt/live/example.com/privkey.pem";

    assert(config.enabled == false);
    assert(config.is_enabled() == false);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_cert_and_key_files_are_configured_but_not_valid_when_disabled()
  {
    TlsConfig config;

    config.cert_file = "/etc/letsencrypt/live/example.com/fullchain.pem";
    config.key_file = "/etc/letsencrypt/live/example.com/privkey.pem";

    assert(config.enabled == false);
    assert(config.is_enabled() == false);
    assert(config.is_configured() == true);
    assert(config.is_valid() == false);
  }

  static void test_enabled_with_cert_and_key_is_valid()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "/etc/letsencrypt/live/example.com/fullchain.pem";
    config.key_file = "/etc/letsencrypt/live/example.com/privkey.pem";

    assert(config.is_enabled() == true);
    assert(config.is_configured() == true);
    assert(config.is_valid() == true);
  }

  static void test_empty_strings_make_config_invalid_even_when_enabled()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "";
    config.key_file = "";

    assert(config.is_enabled() == true);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_clearing_cert_file_makes_config_invalid()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "/cert.pem";
    config.key_file = "/key.pem";

    assert(config.is_valid() == true);

    config.cert_file.clear();

    assert(config.is_enabled() == true);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_clearing_key_file_makes_config_invalid()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "/cert.pem";
    config.key_file = "/key.pem";

    assert(config.is_valid() == true);

    config.key_file.clear();

    assert(config.is_enabled() == true);
    assert(config.is_configured() == false);
    assert(config.is_valid() == false);
  }

  static void test_disabling_valid_config_makes_it_invalid()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "/cert.pem";
    config.key_file = "/key.pem";

    assert(config.is_valid() == true);

    config.enabled = false;

    assert(config.is_enabled() == false);
    assert(config.is_configured() == true);
    assert(config.is_valid() == false);
  }

  static void test_whitespace_paths_are_considered_configured()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = " ";
    config.key_file = "\t";

    assert(config.is_enabled() == true);
    assert(config.is_configured() == true);
    assert(config.is_valid() == true);
  }

  static void test_relative_paths_are_supported()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "certs/dev-cert.pem";
    config.key_file = "certs/dev-key.pem";

    assert(config.is_enabled() == true);
    assert(config.is_configured() == true);
    assert(config.is_valid() == true);
  }

  static void test_absolute_paths_are_supported()
  {
    TlsConfig config;

    config.enabled = true;
    config.cert_file = "/etc/ssl/certs/vix.pem";
    config.key_file = "/etc/ssl/private/vix.key";

    assert(config.is_enabled() == true);
    assert(config.is_configured() == true);
    assert(config.is_valid() == true);
  }

  static void test_aggregate_initialization_disabled()
  {
    TlsConfig config{
        .enabled = false,
        .cert_file = "/cert.pem",
        .key_file = "/key.pem",
    };

    assert(config.enabled == false);
    assert(config.cert_file == "/cert.pem");
    assert(config.key_file == "/key.pem");

    assert(config.is_enabled() == false);
    assert(config.is_configured() == true);
    assert(config.is_valid() == false);
  }

  static void test_aggregate_initialization_enabled()
  {
    TlsConfig config{
        .enabled = true,
        .cert_file = "/cert.pem",
        .key_file = "/key.pem",
    };

    assert(config.enabled == true);
    assert(config.cert_file == "/cert.pem");
    assert(config.key_file == "/key.pem");

    assert(config.is_enabled() == true);
    assert(config.is_configured() == true);
    assert(config.is_valid() == true);
  }

  static void test_copy_preserves_values()
  {
    TlsConfig source{
        .enabled = true,
        .cert_file = "/copy/cert.pem",
        .key_file = "/copy/key.pem",
    };

    TlsConfig copy = source;

    assert(copy.enabled == true);
    assert(copy.cert_file == "/copy/cert.pem");
    assert(copy.key_file == "/copy/key.pem");
    assert(copy.is_valid() == true);

    copy.enabled = false;
    copy.cert_file = "/changed/cert.pem";
    copy.key_file = "/changed/key.pem";

    assert(source.enabled == true);
    assert(source.cert_file == "/copy/cert.pem");
    assert(source.key_file == "/copy/key.pem");
    assert(source.is_valid() == true);

    assert(copy.enabled == false);
    assert(copy.cert_file == "/changed/cert.pem");
    assert(copy.key_file == "/changed/key.pem");
    assert(copy.is_valid() == false);
  }

  static void test_copy_assignment_preserves_values()
  {
    TlsConfig source{
        .enabled = true,
        .cert_file = "/source/cert.pem",
        .key_file = "/source/key.pem",
    };

    TlsConfig target{
        .enabled = false,
        .cert_file = "/old/cert.pem",
        .key_file = "/old/key.pem",
    };

    target = source;

    assert(target.enabled == true);
    assert(target.cert_file == "/source/cert.pem");
    assert(target.key_file == "/source/key.pem");
    assert(target.is_valid() == true);
  }

  static void test_move_preserves_values()
  {
    TlsConfig source{
        .enabled = true,
        .cert_file = "/move/cert.pem",
        .key_file = "/move/key.pem",
    };

    TlsConfig moved = std::move(source);

    assert(moved.enabled == true);
    assert(moved.cert_file == "/move/cert.pem");
    assert(moved.key_file == "/move/key.pem");
    assert(moved.is_valid() == true);
  }

  static void test_move_assignment_preserves_values()
  {
    TlsConfig source{
        .enabled = true,
        .cert_file = "/move-assign/cert.pem",
        .key_file = "/move-assign/key.pem",
    };

    TlsConfig target;

    target = std::move(source);

    assert(target.enabled == true);
    assert(target.cert_file == "/move-assign/cert.pem");
    assert(target.key_file == "/move-assign/key.pem");
    assert(target.is_valid() == true);
  }

  static void test_type_traits()
  {
    static_assert(std::is_default_constructible_v<TlsConfig>);
    static_assert(std::is_copy_constructible_v<TlsConfig>);
    static_assert(std::is_copy_assignable_v<TlsConfig>);
    static_assert(std::is_move_constructible_v<TlsConfig>);
    static_assert(std::is_move_assignable_v<TlsConfig>);
    static_assert(std::is_destructible_v<TlsConfig>);
  }

} // namespace

int main()
{
  test_default_tls_config_is_disabled_and_invalid();

  test_enabled_without_files_is_not_configured_and_invalid();
  test_cert_file_without_key_file_is_not_configured();
  test_key_file_without_cert_file_is_not_configured();
  test_cert_and_key_files_are_configured_but_not_valid_when_disabled();
  test_enabled_with_cert_and_key_is_valid();

  test_empty_strings_make_config_invalid_even_when_enabled();
  test_clearing_cert_file_makes_config_invalid();
  test_clearing_key_file_makes_config_invalid();
  test_disabling_valid_config_makes_it_invalid();

  test_whitespace_paths_are_considered_configured();
  test_relative_paths_are_supported();
  test_absolute_paths_are_supported();

  test_aggregate_initialization_disabled();
  test_aggregate_initialization_enabled();

  test_copy_preserves_values();
  test_copy_assignment_preserves_values();
  test_move_preserves_values();
  test_move_assignment_preserves_values();

  test_type_traits();

  return 0;
}
