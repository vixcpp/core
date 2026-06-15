/**
 *
 * @file runtime_executor_constructor_test.cpp
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
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/runtime/Budget.hpp>
#include <vix/runtime/Runtime.hpp>

namespace
{
  using BudgetConfig = vix::runtime::BudgetConfig;
  using Metrics = vix::executor::Metrics;
  using Runtime = vix::runtime::Runtime;
  using RuntimeConfig = vix::runtime::RuntimeConfig;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;

  static std::uint32_t expected_auto_worker_count()
  {
    const unsigned int hardware = std::thread::hardware_concurrency();

    if (hardware == 0u)
    {
      return 1u;
    }

    return static_cast<std::uint32_t>(hardware);
  }

  static void assert_fresh_executor_state(const RuntimeExecutor &executor)
  {
    assert(executor.started() == false);
    assert(executor.running() == false);
    assert(executor.accepting() == false);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);
  }

  static void test_runtime_executor_type_traits()
  {
    static_assert(std::is_default_constructible_v<RuntimeExecutor>);

    static_assert(std::is_constructible_v<RuntimeExecutor, RuntimeConfig>);
    static_assert(std::is_constructible_v<RuntimeExecutor, std::uint32_t>);
    static_assert(std::is_constructible_v<RuntimeExecutor, std::unique_ptr<Runtime>>);

    static_assert(!std::is_copy_constructible_v<RuntimeExecutor>);
    static_assert(!std::is_copy_assignable_v<RuntimeExecutor>);

    static_assert(!std::is_move_constructible_v<RuntimeExecutor>);
    static_assert(!std::is_move_assignable_v<RuntimeExecutor>);

    static_assert(std::is_destructible_v<RuntimeExecutor>);
  }

  static void test_default_constructor_creates_stopped_executor()
  {
    RuntimeExecutor executor;

    assert_fresh_executor_state(executor);

    assert(executor.runtime().worker_count() == expected_auto_worker_count());
    assert(executor.runtime().config().workerCount == expected_auto_worker_count());
    assert(executor.runtime().config().budget.quantum == 64u);

    executor.stop();
  }

  static void test_runtime_config_constructor_preserves_explicit_worker_count()
  {
    RuntimeConfig config{
        2u,
        BudgetConfig{8u}};

    RuntimeExecutor executor{config};

    assert_fresh_executor_state(executor);

    assert(executor.runtime().worker_count() == 2u);
    assert(executor.runtime().config().workerCount == 2u);
    assert(executor.runtime().config().budget.quantum == 8u);

    executor.stop();
  }

  static void test_runtime_config_constructor_normalizes_zero_worker_count()
  {
    RuntimeConfig config{
        0u,
        BudgetConfig{9u}};

    RuntimeExecutor executor{config};

    assert_fresh_executor_state(executor);

    assert(executor.runtime().worker_count() == expected_auto_worker_count());
    assert(executor.runtime().config().workerCount == expected_auto_worker_count());
    assert(executor.runtime().config().budget.quantum == 9u);

    executor.stop();
  }

  static void test_runtime_config_constructor_normalizes_zero_budget()
  {
    RuntimeConfig config{
        1u,
        BudgetConfig{0u}};

    RuntimeExecutor executor{config};

    assert_fresh_executor_state(executor);

    assert(executor.runtime().worker_count() == 1u);
    assert(executor.runtime().config().workerCount == 1u);
    assert(executor.runtime().config().budget.quantum == 1u);

    executor.stop();
  }

  static void test_worker_count_constructor_preserves_positive_worker_count()
  {
    RuntimeExecutor executor{3u};

    assert_fresh_executor_state(executor);

    assert(executor.runtime().worker_count() == 3u);
    assert(executor.runtime().config().workerCount == 3u);
    assert(executor.runtime().config().budget.quantum == 16u);

    executor.stop();
  }

  static void test_worker_count_constructor_normalizes_zero_to_one()
  {
    RuntimeExecutor executor{0u};

    assert_fresh_executor_state(executor);

    assert(executor.runtime().worker_count() == 1u);
    assert(executor.runtime().config().workerCount == 1u);
    assert(executor.runtime().config().budget.quantum == 16u);

    executor.stop();
  }

  static void test_unique_runtime_constructor_accepts_valid_runtime()
  {
    auto runtime = std::make_unique<Runtime>(
        RuntimeConfig{
            2u,
            BudgetConfig{11u}});

    Runtime *raw_runtime = runtime.get();

    RuntimeExecutor executor{std::move(runtime)};

    assert_fresh_executor_state(executor);

    assert(&executor.runtime() == raw_runtime);
    assert(executor.runtime().worker_count() == 2u);
    assert(executor.runtime().config().workerCount == 2u);
    assert(executor.runtime().config().budget.quantum == 11u);

    executor.stop();
  }

  static void test_unique_runtime_constructor_rejects_null_runtime()
  {
    std::unique_ptr<Runtime> runtime{};

    bool threw = false;

    try
    {
      RuntimeExecutor executor{std::move(runtime)};

      (void)executor;
    }
    catch (const std::invalid_argument &e)
    {
      threw = true;

      const std::string message = e.what();

      assert(message.find("RuntimeExecutor requires a valid runtime") != std::string::npos);
    }

    assert(threw);
  }

  static void test_runtime_accessors_return_same_runtime()
  {
    RuntimeExecutor executor{2u};

    Runtime &first = executor.runtime();
    Runtime &second = executor.runtime();

    const RuntimeExecutor &const_executor = executor;

    const Runtime &const_first = const_executor.runtime();
    const Runtime &const_second = const_executor.runtime();

    assert(&first == &second);
    assert(&first == &const_first);
    assert(&const_first == &const_second);

    assert(first.worker_count() == 2u);
    assert(const_first.worker_count() == 2u);

    executor.stop();
  }

  static void test_constructor_does_not_start_runtime()
  {
    {
      RuntimeExecutor executor;

      assert(executor.started() == false);
      assert(executor.running() == false);
      assert(executor.accepting() == false);
      assert(executor.runtime().started() == false);
      assert(executor.runtime().running() == false);
    }

    {
      RuntimeExecutor executor{1u};

      assert(executor.started() == false);
      assert(executor.running() == false);
      assert(executor.accepting() == false);
      assert(executor.runtime().started() == false);
      assert(executor.runtime().running() == false);
    }

    {
      RuntimeExecutor executor{
          RuntimeConfig{
              1u,
              BudgetConfig{3u}}};

      assert(executor.started() == false);
      assert(executor.running() == false);
      assert(executor.accepting() == false);
      assert(executor.runtime().started() == false);
      assert(executor.runtime().running() == false);
    }
  }

  static void test_metrics_are_zero_after_construction()
  {
    RuntimeExecutor executor{1u};

    const Metrics metrics = executor.metrics();

    assert(metrics.pending == 0u);
    assert(metrics.active == 0u);
    assert(metrics.timed_out == 0u);

    assert(executor.submitted_tasks() == 0u);
    assert(executor.rejected_tasks() == 0u);
    assert(executor.fast_http_submitted_tasks() == 0u);
    assert(executor.failed_tasks() == 0u);

    executor.stop();
  }

  static void test_stop_is_safe_immediately_after_construction()
  {
    RuntimeExecutor executor{1u};

    assert_fresh_executor_state(executor);

    executor.stop();
    executor.stop();
    executor.stop();

    assert_fresh_executor_state(executor);
  }

  static void test_stop_and_wait_is_safe_immediately_after_construction()
  {
    RuntimeExecutor executor{1u};

    assert_fresh_executor_state(executor);

    executor.stop_and_wait();
    executor.stop_and_wait();

    assert_fresh_executor_state(executor);
  }

  static void test_wait_idle_is_safe_immediately_after_construction()
  {
    RuntimeExecutor executor{1u};

    assert_fresh_executor_state(executor);

    executor.wait_idle();

    assert_fresh_executor_state(executor);
  }

  static void test_destructor_is_safe_for_never_started_executor()
  {
    {
      RuntimeExecutor executor;
      assert_fresh_executor_state(executor);
    }

    {
      RuntimeExecutor executor{1u};
      assert_fresh_executor_state(executor);
    }

    {
      auto runtime = std::make_unique<Runtime>(
          RuntimeConfig{
              1u,
              BudgetConfig{4u}});

      RuntimeExecutor executor{std::move(runtime)};
      assert_fresh_executor_state(executor);
    }
  }

  static void test_multiple_executors_have_independent_runtime_instances()
  {
    RuntimeExecutor first{1u};
    RuntimeExecutor second{2u};

    assert(&first.runtime() != &second.runtime());

    assert(first.runtime().worker_count() == 1u);
    assert(second.runtime().worker_count() == 2u);

    assert_fresh_executor_state(first);
    assert_fresh_executor_state(second);

    first.stop();
    second.stop();
  }

  static void test_multiple_executors_have_independent_metrics()
  {
    RuntimeExecutor first{1u};
    RuntimeExecutor second{1u};

    assert(first.submitted_tasks() == 0u);
    assert(first.rejected_tasks() == 0u);

    assert(second.submitted_tasks() == 0u);
    assert(second.rejected_tasks() == 0u);

    const bool first_rejected = first.submit(vix::runtime::TaskFn{});

    assert(!first_rejected);

    assert(first.rejected_tasks() == 1u);
    assert(second.rejected_tasks() == 0u);

    first.stop();
    second.stop();
  }

} // namespace

int main()
{
  test_runtime_executor_type_traits();

  test_default_constructor_creates_stopped_executor();

  test_runtime_config_constructor_preserves_explicit_worker_count();
  test_runtime_config_constructor_normalizes_zero_worker_count();
  test_runtime_config_constructor_normalizes_zero_budget();

  test_worker_count_constructor_preserves_positive_worker_count();
  test_worker_count_constructor_normalizes_zero_to_one();

  test_unique_runtime_constructor_accepts_valid_runtime();
  test_unique_runtime_constructor_rejects_null_runtime();

  test_runtime_accessors_return_same_runtime();

  test_constructor_does_not_start_runtime();
  test_metrics_are_zero_after_construction();

  test_stop_is_safe_immediately_after_construction();
  test_stop_and_wait_is_safe_immediately_after_construction();
  test_wait_idle_is_safe_immediately_after_construction();

  test_destructor_is_safe_for_never_started_executor();

  test_multiple_executors_have_independent_runtime_instances();
  test_multiple_executors_have_independent_metrics();

  return 0;
}
