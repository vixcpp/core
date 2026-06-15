/**
 *
 * @file task_options_test.cpp
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
#include <limits>
#include <type_traits>
#include <utility>

#include <vix/executor/TaskOptions.hpp>

namespace
{
  using TaskOptions = vix::executor::TaskOptions;

  using namespace std::chrono_literals;

  static void test_default_task_options()
  {
    TaskOptions options;

    assert(options.priority == 0);
    assert(options.timeout == 0ms);
    assert(options.deadline == 0ms);
    assert(options.may_block == false);
  }

  static void test_aggregate_initialization()
  {
    TaskOptions options{
        .priority = 10,
        .timeout = 250ms,
        .deadline = 1000ms,
        .may_block = true,
    };

    assert(options.priority == 10);
    assert(options.timeout == 250ms);
    assert(options.deadline == 1000ms);
    assert(options.may_block == true);
  }

  static void test_priority_can_be_positive_zero_or_negative()
  {
    TaskOptions options;

    options.priority = 5;
    assert(options.priority == 5);

    options.priority = 0;
    assert(options.priority == 0);

    options.priority = -5;
    assert(options.priority == -5);
  }

  static void test_priority_extreme_values_are_supported()
  {
    TaskOptions options;

    options.priority = std::numeric_limits<int>::max();
    assert(options.priority == std::numeric_limits<int>::max());

    options.priority = std::numeric_limits<int>::min();
    assert(options.priority == std::numeric_limits<int>::min());
  }

  static void test_timeout_default_zero_means_disabled()
  {
    TaskOptions options;

    assert(options.timeout.count() == 0);
    assert(options.timeout == std::chrono::milliseconds{0});
  }

  static void test_timeout_can_be_set()
  {
    TaskOptions options;

    options.timeout = 1ms;
    assert(options.timeout == 1ms);

    options.timeout = 500ms;
    assert(options.timeout == 500ms);

    options.timeout = std::chrono::milliseconds{10'000};
    assert(options.timeout.count() == 10'000);
  }

  static void test_timeout_can_be_negative_because_it_is_plain_chrono_value()
  {
    TaskOptions options;

    options.timeout = std::chrono::milliseconds{-1};

    assert(options.timeout.count() == -1);
    assert(options.timeout < 0ms);
  }

  static void test_deadline_default_zero_means_disabled()
  {
    TaskOptions options;

    assert(options.deadline.count() == 0);
    assert(options.deadline == std::chrono::milliseconds{0});
  }

  static void test_deadline_can_be_set()
  {
    TaskOptions options;

    options.deadline = 1ms;
    assert(options.deadline == 1ms);

    options.deadline = 1500ms;
    assert(options.deadline == 1500ms);

    options.deadline = std::chrono::milliseconds{60'000};
    assert(options.deadline.count() == 60'000);
  }

  static void test_deadline_can_be_negative_because_it_is_plain_chrono_value()
  {
    TaskOptions options;

    options.deadline = std::chrono::milliseconds{-10};

    assert(options.deadline.count() == -10);
    assert(options.deadline < 0ms);
  }

  static void test_timeout_and_deadline_are_independent()
  {
    TaskOptions options;

    options.timeout = 100ms;

    assert(options.timeout == 100ms);
    assert(options.deadline == 0ms);

    options.deadline = 200ms;

    assert(options.timeout == 100ms);
    assert(options.deadline == 200ms);

    options.timeout = 0ms;

    assert(options.timeout == 0ms);
    assert(options.deadline == 200ms);
  }

  static void test_may_block_default_false()
  {
    TaskOptions options;

    assert(options.may_block == false);
  }

  static void test_may_block_can_be_enabled_and_disabled()
  {
    TaskOptions options;

    options.may_block = true;
    assert(options.may_block == true);

    options.may_block = false;
    assert(options.may_block == false);
  }

  static void test_all_fields_are_independent()
  {
    TaskOptions options;

    options.priority = 9;

    assert(options.priority == 9);
    assert(options.timeout == 0ms);
    assert(options.deadline == 0ms);
    assert(options.may_block == false);

    options.timeout = 300ms;

    assert(options.priority == 9);
    assert(options.timeout == 300ms);
    assert(options.deadline == 0ms);
    assert(options.may_block == false);

    options.deadline = 900ms;

    assert(options.priority == 9);
    assert(options.timeout == 300ms);
    assert(options.deadline == 900ms);
    assert(options.may_block == false);

    options.may_block = true;

    assert(options.priority == 9);
    assert(options.timeout == 300ms);
    assert(options.deadline == 900ms);
    assert(options.may_block == true);
  }

  static void test_copy_constructor_preserves_values()
  {
    TaskOptions source{
        .priority = 3,
        .timeout = 50ms,
        .deadline = 75ms,
        .may_block = true,
    };

    TaskOptions copy = source;

    assert(copy.priority == 3);
    assert(copy.timeout == 50ms);
    assert(copy.deadline == 75ms);
    assert(copy.may_block == true);

    copy.priority = 99;
    copy.timeout = 1ms;
    copy.deadline = 2ms;
    copy.may_block = false;

    assert(source.priority == 3);
    assert(source.timeout == 50ms);
    assert(source.deadline == 75ms);
    assert(source.may_block == true);

    assert(copy.priority == 99);
    assert(copy.timeout == 1ms);
    assert(copy.deadline == 2ms);
    assert(copy.may_block == false);
  }

  static void test_copy_assignment_preserves_values()
  {
    TaskOptions source{
        .priority = 7,
        .timeout = 700ms,
        .deadline = 900ms,
        .may_block = true,
    };

    TaskOptions target{
        .priority = -1,
        .timeout = 1ms,
        .deadline = 2ms,
        .may_block = false,
    };

    target = source;

    assert(target.priority == 7);
    assert(target.timeout == 700ms);
    assert(target.deadline == 900ms);
    assert(target.may_block == true);
  }

  static void test_move_constructor_preserves_values()
  {
    TaskOptions source{
        .priority = 8,
        .timeout = 80ms,
        .deadline = 800ms,
        .may_block = true,
    };

    TaskOptions moved = std::move(source);

    assert(moved.priority == 8);
    assert(moved.timeout == 80ms);
    assert(moved.deadline == 800ms);
    assert(moved.may_block == true);
  }

  static void test_move_assignment_preserves_values()
  {
    TaskOptions source{
        .priority = 11,
        .timeout = 110ms,
        .deadline = 1110ms,
        .may_block = true,
    };

    TaskOptions target;

    target = std::move(source);

    assert(target.priority == 11);
    assert(target.timeout == 110ms);
    assert(target.deadline == 1110ms);
    assert(target.may_block == true);
  }

  static void test_large_timeout_and_deadline_values_are_supported()
  {
    TaskOptions options;

    const auto max_count = std::numeric_limits<std::chrono::milliseconds::rep>::max();

    options.timeout = std::chrono::milliseconds{max_count};
    options.deadline = std::chrono::milliseconds{max_count - 1};

    assert(options.timeout.count() == max_count);
    assert(options.deadline.count() == max_count - 1);
  }

  static void test_zero_values_are_supported_explicitly()
  {
    TaskOptions options{
        .priority = 0,
        .timeout = 0ms,
        .deadline = 0ms,
        .may_block = false,
    };

    assert(options.priority == 0);
    assert(options.timeout == 0ms);
    assert(options.deadline == 0ms);
    assert(options.may_block == false);
  }

  static void test_blocking_low_priority_task_options()
  {
    TaskOptions options{
        .priority = -10,
        .timeout = 5000ms,
        .deadline = 10000ms,
        .may_block = true,
    };

    assert(options.priority == -10);
    assert(options.timeout == 5000ms);
    assert(options.deadline == 10000ms);
    assert(options.may_block == true);
  }

  static void test_fast_high_priority_task_options()
  {
    TaskOptions options{
        .priority = 100,
        .timeout = 5ms,
        .deadline = 10ms,
        .may_block = false,
    };

    assert(options.priority == 100);
    assert(options.timeout == 5ms);
    assert(options.deadline == 10ms);
    assert(options.may_block == false);
  }

  static void test_task_options_type_traits()
  {
    static_assert(std::is_default_constructible_v<TaskOptions>);
    static_assert(std::is_copy_constructible_v<TaskOptions>);
    static_assert(std::is_copy_assignable_v<TaskOptions>);
    static_assert(std::is_move_constructible_v<TaskOptions>);
    static_assert(std::is_move_assignable_v<TaskOptions>);
    static_assert(std::is_destructible_v<TaskOptions>);

    static_assert(std::is_standard_layout_v<TaskOptions>);
  }

  static void test_task_options_field_types()
  {
    static_assert(std::is_same_v<decltype(TaskOptions{}.priority), int>);
    static_assert(std::is_same_v<decltype(TaskOptions{}.timeout), std::chrono::milliseconds>);
    static_assert(std::is_same_v<decltype(TaskOptions{}.deadline), std::chrono::milliseconds>);
    static_assert(std::is_same_v<decltype(TaskOptions{}.may_block), bool>);
  }

} // namespace

int main()
{
  test_default_task_options();

  test_aggregate_initialization();

  test_priority_can_be_positive_zero_or_negative();
  test_priority_extreme_values_are_supported();

  test_timeout_default_zero_means_disabled();
  test_timeout_can_be_set();
  test_timeout_can_be_negative_because_it_is_plain_chrono_value();

  test_deadline_default_zero_means_disabled();
  test_deadline_can_be_set();
  test_deadline_can_be_negative_because_it_is_plain_chrono_value();

  test_timeout_and_deadline_are_independent();

  test_may_block_default_false();
  test_may_block_can_be_enabled_and_disabled();

  test_all_fields_are_independent();

  test_copy_constructor_preserves_values();
  test_copy_assignment_preserves_values();
  test_move_constructor_preserves_values();
  test_move_assignment_preserves_values();

  test_large_timeout_and_deadline_values_are_supported();
  test_zero_values_are_supported_explicitly();

  test_blocking_low_priority_task_options();
  test_fast_high_priority_task_options();

  test_task_options_type_traits();
  test_task_options_field_types();

  return 0;
}
