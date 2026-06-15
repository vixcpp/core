/**
 *
 * @file budget_test.cpp
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

#include <vix/runtime/Budget.hpp>

namespace
{
  using Budget = vix::runtime::Budget;
  using BudgetConfig = vix::runtime::BudgetConfig;

  static void test_budget_config_default_quantum()
  {
    BudgetConfig config;

    assert(config.quantum == 64u);
  }

  static void test_budget_config_custom_quantum()
  {
    BudgetConfig config{128u};

    assert(config.quantum == 128u);
  }

  static void test_budget_config_normalizes_zero_to_one()
  {
    BudgetConfig config{0u};

    assert(config.quantum == 1u);
  }

  static void test_budget_config_normalize_quantum()
  {
    static_assert(BudgetConfig::normalize_quantum(0u) == 1u);
    static_assert(BudgetConfig::normalize_quantum(1u) == 1u);
    static_assert(BudgetConfig::normalize_quantum(64u) == 64u);
    static_assert(BudgetConfig::normalize_quantum(1024u) == 1024u);
  }

  static void test_budget_config_accepts_max_uint32()
  {
    constexpr std::uint32_t max_value = std::numeric_limits<std::uint32_t>::max();

    BudgetConfig config{max_value};

    assert(config.quantum == max_value);
    assert(BudgetConfig::normalize_quantum(max_value) == max_value);
  }

  static void test_default_budget_initial_state()
  {
    Budget budget;

    assert(budget.limit() == 64u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 64u);

    assert(budget.empty());
    assert(budget.available());
    assert(!budget.exhausted());
    assert(!budget.full());
    assert(!budget.should_yield());
  }

  static void test_custom_budget_initial_state()
  {
    Budget budget{BudgetConfig{5u}};

    assert(budget.limit() == 5u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 5u);

    assert(budget.empty());
    assert(budget.available());
    assert(!budget.exhausted());
    assert(!budget.full());
    assert(!budget.should_yield());
  }

  static void test_zero_quantum_budget_is_normalized()
  {
    Budget budget{BudgetConfig{0u}};

    assert(budget.limit() == 1u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 1u);

    assert(budget.available());
    assert(!budget.exhausted());
  }

  static void test_consume_one_step_keeps_capacity_when_not_exhausted()
  {
    Budget budget{BudgetConfig{3u}};

    const bool after_first = budget.consume();

    assert(after_first == true);
    assert(budget.limit() == 3u);
    assert(budget.used() == 1u);
    assert(budget.remaining() == 2u);

    assert(!budget.empty());
    assert(budget.available());
    assert(!budget.exhausted());
    assert(!budget.full());
    assert(!budget.should_yield());
  }

  static void test_consume_until_exact_exhaustion()
  {
    Budget budget{BudgetConfig{3u}};

    assert(budget.consume() == true);
    assert(budget.used() == 1u);
    assert(budget.remaining() == 2u);

    assert(budget.consume() == true);
    assert(budget.used() == 2u);
    assert(budget.remaining() == 1u);

    assert(budget.consume() == false);
    assert(budget.used() == 3u);
    assert(budget.remaining() == 0u);

    assert(!budget.empty());
    assert(!budget.available());
    assert(budget.exhausted());
    assert(budget.full());
    assert(budget.should_yield());
  }

  static void test_consume_after_exhaustion_stays_exhausted()
  {
    Budget budget{BudgetConfig{2u}};

    assert(budget.consume() == true);
    assert(budget.consume() == false);

    assert(budget.used() == 2u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());

    assert(budget.consume() == false);
    assert(budget.consume() == false);

    assert(budget.used() == 2u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
  }

  static void test_consume_zero_steps_does_not_change_state()
  {
    Budget budget{BudgetConfig{4u}};

    assert(budget.consume(0u) == true);

    assert(budget.used() == 0u);
    assert(budget.remaining() == 4u);
    assert(budget.empty());
    assert(budget.available());
    assert(!budget.exhausted());
  }

  static void test_consume_zero_steps_on_exhausted_budget_returns_false()
  {
    Budget budget{BudgetConfig{1u}};

    assert(budget.consume() == false);
    assert(budget.exhausted());

    assert(budget.consume(0u) == false);

    assert(budget.used() == 1u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
  }

  static void test_consume_multiple_steps_without_exhaustion()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(3u) == true);

    assert(budget.used() == 3u);
    assert(budget.remaining() == 7u);
    assert(budget.available());
    assert(!budget.exhausted());
  }

  static void test_consume_multiple_steps_to_exact_exhaustion()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(10u) == false);

    assert(budget.used() == 10u);
    assert(budget.remaining() == 0u);
    assert(!budget.available());
    assert(budget.exhausted());
    assert(budget.should_yield());
  }

  static void test_consume_multiple_steps_saturates_when_over_limit()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(99u) == false);

    assert(budget.used() == 10u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
  }

  static void test_consume_multiple_steps_saturates_from_partial_usage()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(4u) == true);

    assert(budget.used() == 4u);
    assert(budget.remaining() == 6u);

    assert(budget.consume(6u) == false);

    assert(budget.used() == 10u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
  }

  static void test_consume_more_than_remaining_saturates()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(7u) == true);

    assert(budget.used() == 7u);
    assert(budget.remaining() == 3u);

    assert(budget.consume(4u) == false);

    assert(budget.used() == 10u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
  }

  static void test_exhaust_forces_exhausted_state()
  {
    Budget budget{BudgetConfig{8u}};

    assert(budget.used() == 0u);
    assert(budget.remaining() == 8u);

    budget.exhaust();

    assert(budget.used() == 8u);
    assert(budget.remaining() == 0u);

    assert(!budget.available());
    assert(budget.exhausted());
    assert(budget.full());
    assert(budget.should_yield());
  }

  static void test_exhaust_after_partial_consumption()
  {
    Budget budget{BudgetConfig{8u}};

    assert(budget.consume(3u) == true);

    assert(budget.used() == 3u);
    assert(budget.remaining() == 5u);

    budget.exhaust();

    assert(budget.used() == 8u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
  }

  static void test_reset_restores_empty_state_without_changing_limit()
  {
    Budget budget{BudgetConfig{6u}};

    assert(budget.consume(6u) == false);
    assert(budget.exhausted());

    budget.reset();

    assert(budget.limit() == 6u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 6u);

    assert(budget.empty());
    assert(budget.available());
    assert(!budget.exhausted());
    assert(!budget.full());
    assert(!budget.should_yield());
  }

  static void test_reset_after_exhaust()
  {
    Budget budget{BudgetConfig{2u}};

    budget.exhaust();

    assert(budget.used() == 2u);
    assert(budget.exhausted());

    budget.reset();

    assert(budget.used() == 0u);
    assert(budget.remaining() == 2u);
    assert(budget.available());
  }

  static void test_reconfigure_changes_limit_and_resets_used()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(7u) == true);

    assert(budget.limit() == 10u);
    assert(budget.used() == 7u);
    assert(budget.remaining() == 3u);

    budget.reconfigure(BudgetConfig{3u});

    assert(budget.limit() == 3u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 3u);

    assert(budget.empty());
    assert(budget.available());
    assert(!budget.exhausted());
  }

  static void test_reconfigure_zero_quantum_normalizes_to_one()
  {
    Budget budget{BudgetConfig{10u}};

    assert(budget.consume(5u) == true);

    budget.reconfigure(BudgetConfig{0u});

    assert(budget.limit() == 1u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 1u);

    assert(budget.consume() == false);
    assert(budget.used() == 1u);
    assert(budget.exhausted());
  }

  static void test_reconfigure_after_exhaustion_restores_capacity()
  {
    Budget budget{BudgetConfig{2u}};

    assert(budget.consume(2u) == false);
    assert(budget.exhausted());

    budget.reconfigure(BudgetConfig{5u});

    assert(budget.limit() == 5u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 5u);
    assert(budget.available());
  }

  static void test_budget_with_limit_one()
  {
    Budget budget{BudgetConfig{1u}};

    assert(budget.limit() == 1u);
    assert(budget.used() == 0u);
    assert(budget.remaining() == 1u);

    assert(budget.consume() == false);

    assert(budget.used() == 1u);
    assert(budget.remaining() == 0u);
    assert(budget.exhausted());
    assert(budget.should_yield());
  }

  static void test_budget_copy_preserves_state()
  {
    Budget budget{BudgetConfig{5u}};

    assert(budget.consume(2u) == true);

    Budget copy = budget;

    assert(copy.limit() == 5u);
    assert(copy.used() == 2u);
    assert(copy.remaining() == 3u);

    assert(budget.limit() == 5u);
    assert(budget.used() == 2u);
    assert(budget.remaining() == 3u);

    assert(copy.consume(3u) == false);

    assert(copy.exhausted());
    assert(!budget.exhausted());
  }

  static void test_budget_copy_assignment_preserves_state()
  {
    Budget source{BudgetConfig{9u}};
    Budget target{BudgetConfig{2u}};

    assert(source.consume(4u) == true);
    assert(target.consume(1u) == true);

    target = source;

    assert(target.limit() == 9u);
    assert(target.used() == 4u);
    assert(target.remaining() == 5u);

    assert(source.limit() == 9u);
    assert(source.used() == 4u);
    assert(source.remaining() == 5u);
  }

  static void test_budget_config_type_traits()
  {
    static_assert(std::is_default_constructible_v<BudgetConfig>);
    static_assert(std::is_constructible_v<BudgetConfig, std::uint32_t>);
    static_assert(!std::is_convertible_v<std::uint32_t, BudgetConfig>);

    static_assert(std::is_copy_constructible_v<BudgetConfig>);
    static_assert(std::is_copy_assignable_v<BudgetConfig>);
    static_assert(std::is_move_constructible_v<BudgetConfig>);
    static_assert(std::is_move_assignable_v<BudgetConfig>);
    static_assert(std::is_destructible_v<BudgetConfig>);

    static_assert(noexcept(BudgetConfig{}));
    static_assert(noexcept(BudgetConfig{1u}));
    static_assert(noexcept(BudgetConfig::normalize_quantum(0u)));
  }

  static void test_budget_type_traits()
  {
    static_assert(std::is_constructible_v<Budget>);
    static_assert(std::is_constructible_v<Budget, BudgetConfig>);
    static_assert(std::is_copy_constructible_v<Budget>);
    static_assert(std::is_copy_assignable_v<Budget>);
    static_assert(std::is_move_constructible_v<Budget>);
    static_assert(std::is_move_assignable_v<Budget>);
    static_assert(std::is_destructible_v<Budget>);

    static_assert(noexcept(Budget{}));
    static_assert(noexcept(Budget{BudgetConfig{4u}}));
  }

} // namespace

int main()
{
  test_budget_config_default_quantum();
  test_budget_config_custom_quantum();
  test_budget_config_normalizes_zero_to_one();
  test_budget_config_normalize_quantum();
  test_budget_config_accepts_max_uint32();

  test_default_budget_initial_state();
  test_custom_budget_initial_state();
  test_zero_quantum_budget_is_normalized();

  test_consume_one_step_keeps_capacity_when_not_exhausted();
  test_consume_until_exact_exhaustion();
  test_consume_after_exhaustion_stays_exhausted();

  test_consume_zero_steps_does_not_change_state();
  test_consume_zero_steps_on_exhausted_budget_returns_false();

  test_consume_multiple_steps_without_exhaustion();
  test_consume_multiple_steps_to_exact_exhaustion();
  test_consume_multiple_steps_saturates_when_over_limit();
  test_consume_multiple_steps_saturates_from_partial_usage();
  test_consume_more_than_remaining_saturates();

  test_exhaust_forces_exhausted_state();
  test_exhaust_after_partial_consumption();

  test_reset_restores_empty_state_without_changing_limit();
  test_reset_after_exhaust();

  test_reconfigure_changes_limit_and_resets_used();
  test_reconfigure_zero_quantum_normalizes_to_one();
  test_reconfigure_after_exhaustion_restores_capacity();

  test_budget_with_limit_one();

  test_budget_copy_preserves_state();
  test_budget_copy_assignment_preserves_state();

  test_budget_config_type_traits();
  test_budget_type_traits();

  return 0;
}
