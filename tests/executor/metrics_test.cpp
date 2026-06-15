/**
 *
 * @file metrics_test.cpp
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
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include <vix/executor/Metrics.hpp>

namespace
{
  using Metrics = vix::executor::Metrics;

  static void test_default_metrics_are_zero()
  {
    Metrics metrics;

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_aggregate_initialization()
  {
    Metrics metrics{
        .pending = 10u,
        .active = 3u,
        .timed_out = 1u,
    };

    assert(metrics.pending == 10u);
    assert(metrics.active == 3u);
    assert(metrics.timed_out == 1u);
  }

  static void test_metrics_fields_can_be_mutated()
  {
    Metrics metrics;

    metrics.pending = 5u;
    metrics.active = 2u;
    metrics.timed_out = 1u;

    assert(metrics.pending == 5u);
    assert(metrics.active == 2u);
    assert(metrics.timed_out == 1u);

    metrics.pending = 0u;
    metrics.active = 0u;
    metrics.timed_out = 0u;

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_pending_active_and_timed_out_are_independent()
  {
    Metrics metrics;

    metrics.pending = 100u;

    assert(metrics.pending == 100u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    metrics.active = 50u;

    assert(metrics.pending == 100u);
    assert(metrics.active == 50u);
    assert(metrics.timed_out == 0u);

    metrics.timed_out = 7u;

    assert(metrics.pending == 100u);
    assert(metrics.active == 50u);
    assert(metrics.timed_out == 7u);
  }

  static void test_copy_constructor_preserves_values()
  {
    Metrics source{
        .pending = 12u,
        .active = 4u,
        .timed_out = 2u,
    };

    Metrics copy = source;

    assert(copy.pending == 12u);
    assert(copy.active == 4u);
    assert(copy.timed_out == 2u);

    copy.pending = 99u;
    copy.active = 88u;
    copy.timed_out = 77u;

    assert(source.pending == 12u);
    assert(source.active == 4u);
    assert(source.timed_out == 2u);

    assert(copy.pending == 99u);
    assert(copy.active == 88u);
    assert(copy.timed_out == 77u);
  }

  static void test_copy_assignment_preserves_values()
  {
    Metrics source{
        .pending = 9u,
        .active = 8u,
        .timed_out = 7u,
    };

    Metrics target{
        .pending = 1u,
        .active = 2u,
        .timed_out = 3u,
    };

    target = source;

    assert(target.pending == 9u);
    assert(target.active == 8u);
    assert(target.timed_out == 7u);

    assert(source.pending == 9u);
    assert(source.active == 8u);
    assert(source.timed_out == 7u);
  }

  static void test_move_constructor_preserves_values()
  {
    Metrics source{
        .pending = 30u,
        .active = 20u,
        .timed_out = 10u,
    };

    Metrics moved = std::move(source);

    assert(moved.pending == 30u);
    assert(moved.active == 20u);
    assert(moved.timed_out == 10u);
  }

  static void test_move_assignment_preserves_values()
  {
    Metrics source{
        .pending = 300u,
        .active = 200u,
        .timed_out = 100u,
    };

    Metrics target;

    target = std::move(source);

    assert(target.pending == 300u);
    assert(target.active == 200u);
    assert(target.timed_out == 100u);
  }

  static void test_zero_values_are_supported_explicitly()
  {
    Metrics metrics{
        .pending = 0u,
        .active = 0u,
        .timed_out = 0u,
    };

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_large_values_are_supported()
  {
    constexpr std::uint64_t max_value =
        std::numeric_limits<std::uint64_t>::max();

    Metrics metrics{
        .pending = max_value,
        .active = max_value - 1u,
        .timed_out = max_value - 2u,
    };

    assert(metrics.pending == max_value);
    assert(metrics.active == max_value - 1u);
    assert(metrics.timed_out == max_value - 2u);
  }

  static void test_metrics_can_represent_pending_without_active_work()
  {
    Metrics metrics{
        .pending = 42u,
        .active = 0u,
        .timed_out = 0u,
    };

    assert(metrics.pending == 42u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_metrics_can_represent_active_without_pending_work()
  {
    Metrics metrics{
        .pending = 0u,
        .active = 4u,
        .timed_out = 0u,
    };

    assert(metrics.pending == 0u);
    assert(metrics.active == 4u);
    assert(metrics.timed_out == 0u);
  }

  static void test_metrics_can_represent_timeouts_without_active_work()
  {
    Metrics metrics{
        .pending = 0u,
        .active = 0u,
        .timed_out = 3u,
    };

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 3u);
  }

  static void test_metrics_type_traits()
  {
    static_assert(std::is_default_constructible_v<Metrics>);
    static_assert(std::is_copy_constructible_v<Metrics>);
    static_assert(std::is_copy_assignable_v<Metrics>);
    static_assert(std::is_move_constructible_v<Metrics>);
    static_assert(std::is_move_assignable_v<Metrics>);
    static_assert(std::is_destructible_v<Metrics>);

    static_assert(std::is_standard_layout_v<Metrics>);
    static_assert(std::is_trivially_copyable_v<Metrics>);
  }

  static void test_metrics_field_types_are_uint64()
  {
    static_assert(std::is_same_v<decltype(Metrics{}.pending), std::uint64_t>);
    static_assert(std::is_same_v<decltype(Metrics{}.active), std::uint64_t>);
    static_assert(std::is_same_v<decltype(Metrics{}.timed_out), std::uint64_t>);
  }

} // namespace

int main()
{
  test_default_metrics_are_zero();

  test_aggregate_initialization();
  test_metrics_fields_can_be_mutated();
  test_pending_active_and_timed_out_are_independent();

  test_copy_constructor_preserves_values();
  test_copy_assignment_preserves_values();
  test_move_constructor_preserves_values();
  test_move_assignment_preserves_values();

  test_zero_values_are_supported_explicitly();
  test_large_values_are_supported();

  test_metrics_can_represent_pending_without_active_work();
  test_metrics_can_represent_active_without_pending_work();
  test_metrics_can_represent_timeouts_without_active_work();

  test_metrics_type_traits();
  test_metrics_field_types_are_uint64();

  return 0;
}
