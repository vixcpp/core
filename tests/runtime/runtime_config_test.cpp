/**
 *
 * @file runtime_config_test.cpp
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
#include <thread>
#include <type_traits>

#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Runtime = vix::runtime::Runtime;
  using RuntimeConfig = vix::runtime::RuntimeConfig;

  static std::uint32_t expected_auto_worker_count()
  {
    const unsigned int hardware = std::thread::hardware_concurrency();

    if (hardware == 0u)
    {
      return 1u;
    }

    return static_cast<std::uint32_t>(hardware);
  }

  static void test_runtime_config_default_values()
  {
    RuntimeConfig config;

    assert(config.workerCount == 0u);
    assert(config.budget.quantum == 64u);
  }

  static void test_runtime_config_custom_worker_count()
  {
    RuntimeConfig config{4u};

    assert(config.workerCount == 4u);
    assert(config.budget.quantum == 64u);
  }

  static void test_runtime_config_custom_worker_count_and_budget()
  {
    RuntimeConfig config{
        3u,
        BudgetConfig{128u}};

    assert(config.workerCount == 3u);
    assert(config.budget.quantum == 128u);
  }

  static void test_runtime_config_zero_worker_count_means_auto_before_runtime_normalization()
  {
    RuntimeConfig config{
        0u,
        BudgetConfig{32u}};

    assert(config.workerCount == 0u);
    assert(config.budget.quantum == 32u);
  }

  static void test_runtime_config_budget_zero_is_normalized_by_budget_config()
  {
    RuntimeConfig config{
        2u,
        BudgetConfig{0u}};

    assert(config.workerCount == 2u);
    assert(config.budget.quantum == 1u);
  }

  static void test_runtime_config_can_be_copied()
  {
    RuntimeConfig source{
        5u,
        BudgetConfig{9u}};

    RuntimeConfig copy = source;

    assert(copy.workerCount == 5u);
    assert(copy.budget.quantum == 9u);

    copy.workerCount = 7u;
    copy.budget.quantum = 11u;

    assert(source.workerCount == 5u);
    assert(source.budget.quantum == 9u);

    assert(copy.workerCount == 7u);
    assert(copy.budget.quantum == 11u);
  }

  static void test_runtime_config_copy_assignment()
  {
    RuntimeConfig source{
        6u,
        BudgetConfig{10u}};

    RuntimeConfig target{
        1u,
        BudgetConfig{2u}};

    target = source;

    assert(target.workerCount == 6u);
    assert(target.budget.quantum == 10u);
  }

  static void test_default_runtime_normalizes_auto_worker_count()
  {
    Runtime runtime;

    const std::uint32_t expected = expected_auto_worker_count();

    assert(runtime.config().workerCount == expected);
    assert(runtime.worker_count() == expected);
    assert(runtime.config().budget.quantum == 64u);

    assert(!runtime.started());
    assert(!runtime.running());
    assert(runtime.empty());
    assert(runtime.size() == 0u);

    assert(runtime.submitted_tasks() == 0u);
    assert(runtime.rejected_tasks() == 0u);
  }

  static void test_runtime_normalizes_zero_worker_count_to_auto()
  {
    RuntimeConfig config{
        0u,
        BudgetConfig{8u}};

    Runtime runtime{config};

    const std::uint32_t expected = expected_auto_worker_count();

    assert(runtime.config().workerCount == expected);
    assert(runtime.worker_count() == expected);
    assert(runtime.config().budget.quantum == 8u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_runtime_preserves_explicit_worker_count_one()
  {
    RuntimeConfig config{
        1u,
        BudgetConfig{8u}};

    Runtime runtime{config};

    assert(runtime.config().workerCount == 1u);
    assert(runtime.worker_count() == 1u);
    assert(runtime.config().budget.quantum == 8u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_runtime_preserves_explicit_worker_count_two()
  {
    RuntimeConfig config{
        2u,
        BudgetConfig{16u}};

    Runtime runtime{config};

    assert(runtime.config().workerCount == 2u);
    assert(runtime.worker_count() == 2u);
    assert(runtime.config().budget.quantum == 16u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_runtime_normalizes_zero_budget_quantum_to_one()
  {
    RuntimeConfig config{
        1u,
        BudgetConfig{0u}};

    Runtime runtime{config};

    assert(runtime.config().workerCount == 1u);
    assert(runtime.worker_count() == 1u);
    assert(runtime.config().budget.quantum == 1u);

    assert(!runtime.started());
    assert(!runtime.running());
  }

  static void test_runtime_config_reference_is_stable()
  {
    RuntimeConfig config{
        2u,
        BudgetConfig{33u}};

    Runtime runtime{config};

    const RuntimeConfig &first = runtime.config();
    const RuntimeConfig &second = runtime.config();

    assert(&first == &second);

    assert(first.workerCount == 2u);
    assert(first.budget.quantum == 33u);

    assert(second.workerCount == 2u);
    assert(second.budget.quantum == 33u);
  }

  static void test_runtime_config_does_not_change_after_start_stop()
  {
    RuntimeConfig config{
        1u,
        BudgetConfig{4u}};

    Runtime runtime{config};

    assert(runtime.config().workerCount == 1u);
    assert(runtime.config().budget.quantum == 4u);

    runtime.start();

    assert(runtime.started());
    assert(runtime.running());

    assert(runtime.config().workerCount == 1u);
    assert(runtime.config().budget.quantum == 4u);

    runtime.stop();

    assert(!runtime.started());
    assert(!runtime.running());

    assert(runtime.config().workerCount == 1u);
    assert(runtime.config().budget.quantum == 4u);
  }

  static void test_runtime_worker_count_matches_config_after_restart()
  {
    RuntimeConfig config{
        2u,
        BudgetConfig{7u}};

    Runtime runtime{config};

    assert(runtime.worker_count() == 2u);
    assert(runtime.config().workerCount == 2u);

    runtime.start();

    assert(runtime.worker_count() == 2u);
    assert(runtime.config().workerCount == 2u);

    runtime.stop();

    assert(runtime.worker_count() == 2u);
    assert(runtime.config().workerCount == 2u);

    runtime.start();

    assert(runtime.worker_count() == 2u);
    assert(runtime.config().workerCount == 2u);

    runtime.stop();
  }

  static void test_runtime_config_type_traits()
  {
    static_assert(std::is_constructible_v<RuntimeConfig>);
    static_assert(std::is_constructible_v<RuntimeConfig, std::uint32_t>);
    static_assert(std::is_constructible_v<RuntimeConfig, std::uint32_t, BudgetConfig>);

    static_assert(!std::is_convertible_v<std::uint32_t, RuntimeConfig>);

    static_assert(std::is_copy_constructible_v<RuntimeConfig>);
    static_assert(std::is_copy_assignable_v<RuntimeConfig>);
    static_assert(std::is_move_constructible_v<RuntimeConfig>);
    static_assert(std::is_move_assignable_v<RuntimeConfig>);
    static_assert(std::is_destructible_v<RuntimeConfig>);

    static_assert(noexcept(RuntimeConfig{}));
    static_assert(noexcept(RuntimeConfig{1u}));
    static_assert(noexcept(RuntimeConfig{1u, BudgetConfig{2u}}));
  }

  static void test_runtime_type_traits()
  {
    static_assert(std::is_constructible_v<Runtime>);
    static_assert(std::is_constructible_v<Runtime, RuntimeConfig>);

    static_assert(!std::is_copy_constructible_v<Runtime>);
    static_assert(!std::is_copy_assignable_v<Runtime>);

    static_assert(!std::is_move_constructible_v<Runtime>);
    static_assert(!std::is_move_assignable_v<Runtime>);

    static_assert(std::is_destructible_v<Runtime>);
  }

} // namespace

int main()
{
  test_runtime_config_default_values();
  test_runtime_config_custom_worker_count();
  test_runtime_config_custom_worker_count_and_budget();
  test_runtime_config_zero_worker_count_means_auto_before_runtime_normalization();
  test_runtime_config_budget_zero_is_normalized_by_budget_config();

  test_runtime_config_can_be_copied();
  test_runtime_config_copy_assignment();

  test_default_runtime_normalizes_auto_worker_count();
  test_runtime_normalizes_zero_worker_count_to_auto();

  test_runtime_preserves_explicit_worker_count_one();
  test_runtime_preserves_explicit_worker_count_two();

  test_runtime_normalizes_zero_budget_quantum_to_one();

  test_runtime_config_reference_is_stable();
  test_runtime_config_does_not_change_after_start_stop();
  test_runtime_worker_count_matches_config_after_restart();

  test_runtime_config_type_traits();
  test_runtime_type_traits();

  return 0;
}
