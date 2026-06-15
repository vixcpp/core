/**
 *
 * @file route_options_test.cpp
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
#include <type_traits>

#include <vix/router/RouteOptions.hpp>

namespace
{
  static void test_default_options_are_light()
  {
    vix::router::RouteOptions options;

    assert(options.heavy == false);
  }

  static void test_heavy_option_can_be_enabled()
  {
    vix::router::RouteOptions options;

    options.heavy = true;

    assert(options.heavy == true);
  }

  static void test_heavy_option_can_be_disabled_again()
  {
    vix::router::RouteOptions options;

    options.heavy = true;
    assert(options.heavy == true);

    options.heavy = false;
    assert(options.heavy == false);
  }

  static void test_aggregate_initialization()
  {
    vix::router::RouteOptions light{};

    assert(light.heavy == false);

    vix::router::RouteOptions heavy{
        .heavy = true,
    };

    assert(heavy.heavy == true);
  }

  static void test_copy_preserves_values()
  {
    vix::router::RouteOptions source{
        .heavy = true,
    };

    vix::router::RouteOptions copy = source;

    assert(copy.heavy == true);

    copy.heavy = false;

    assert(source.heavy == true);
    assert(copy.heavy == false);
  }

  static void test_move_preserves_values()
  {
    vix::router::RouteOptions source{
        .heavy = true,
    };

    vix::router::RouteOptions moved = static_cast<vix::router::RouteOptions &&>(source);

    assert(moved.heavy == true);
  }

  static void test_type_traits()
  {
    static_assert(std::is_default_constructible_v<vix::router::RouteOptions>);
    static_assert(std::is_copy_constructible_v<vix::router::RouteOptions>);
    static_assert(std::is_copy_assignable_v<vix::router::RouteOptions>);
    static_assert(std::is_move_constructible_v<vix::router::RouteOptions>);
    static_assert(std::is_move_assignable_v<vix::router::RouteOptions>);
    static_assert(std::is_trivially_destructible_v<vix::router::RouteOptions>);
  }

} // namespace

int main()
{
  test_default_options_are_light();
  test_heavy_option_can_be_enabled();
  test_heavy_option_can_be_disabled_again();
  test_aggregate_initialization();
  test_copy_preserves_values();
  test_move_preserves_values();
  test_type_traits();

  return 0;
}
