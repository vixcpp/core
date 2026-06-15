/**
 *
 * @file supervisor_test.cpp
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
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/runtime/Supervisor.hpp>

namespace
{
  using RestartPolicy = vix::runtime::RestartPolicy;
  using SupervisedComponent = vix::runtime::SupervisedComponent;
  using SupervisedState = vix::runtime::SupervisedState;
  using Supervisor = vix::runtime::Supervisor;

  static SupervisedComponent make_component(
      std::string name,
      int &start_count,
      int &stop_count,
      RestartPolicy policy = RestartPolicy::on_failure)
  {
    return SupervisedComponent{
        std::move(name),
        [&start_count]()
        {
          ++start_count;
          return true;
        },
        [&stop_count]()
        {
          ++stop_count;
        },
        policy};
  }

  static SupervisedComponent make_failing_component(
      std::string name,
      int &start_count,
      int &stop_count,
      RestartPolicy policy = RestartPolicy::on_failure)
  {
    return SupervisedComponent{
        std::move(name),
        [&start_count]()
        {
          ++start_count;
          return false;
        },
        [&stop_count]()
        {
          ++stop_count;
        },
        policy};
  }

  static SupervisedComponent make_throwing_start_component(
      std::string name,
      int &start_count,
      int &stop_count,
      RestartPolicy policy = RestartPolicy::on_failure)
  {
    return SupervisedComponent{
        std::move(name),
        [&start_count]() -> bool
        {
          ++start_count;
          throw std::runtime_error("start failed");
        },
        [&stop_count]()
        {
          ++stop_count;
        },
        policy};
  }

  static SupervisedComponent make_throwing_stop_component(
      std::string name,
      int &start_count,
      int &stop_count,
      RestartPolicy policy = RestartPolicy::on_failure)
  {
    return SupervisedComponent{
        std::move(name),
        [&start_count]()
        {
          ++start_count;
          return true;
        },
        [&stop_count]()
        {
          ++stop_count;
          throw std::runtime_error("stop failed");
        },
        policy};
  }

  static void test_restart_policy_numeric_values_are_stable()
  {
    static_assert(static_cast<std::uint8_t>(RestartPolicy::never) == 0u);
    static_assert(static_cast<std::uint8_t>(RestartPolicy::always) == 1u);
    static_assert(static_cast<std::uint8_t>(RestartPolicy::on_failure) == 2u);
  }

  static void test_supervised_state_numeric_values_are_stable()
  {
    static_assert(static_cast<std::uint8_t>(SupervisedState::stopped) == 0u);
    static_assert(static_cast<std::uint8_t>(SupervisedState::running) == 1u);
    static_assert(static_cast<std::uint8_t>(SupervisedState::failed) == 2u);
  }

  static void test_supervised_component_default_state()
  {
    SupervisedComponent component;

    assert(component.name.empty());
    assert(!component.start);
    assert(!component.stop);
    assert(component.policy == RestartPolicy::never);
    assert(component.state == SupervisedState::stopped);
    assert(component.restartCount == 0u);
    assert(component.valid() == false);
  }

  static void test_supervised_component_valid_when_name_and_callbacks_exist()
  {
    int start_count = 0;
    int stop_count = 0;

    SupervisedComponent component = make_component(
        "database",
        start_count,
        stop_count);

    assert(component.name == "database");
    assert(component.start);
    assert(component.stop);
    assert(component.policy == RestartPolicy::on_failure);
    assert(component.state == SupervisedState::stopped);
    assert(component.restartCount == 0u);
    assert(component.valid() == true);

    assert(start_count == 0);
    assert(stop_count == 0);
  }

  static void test_supervised_component_invalid_without_name()
  {
    SupervisedComponent component{
        "",
        []()
        {
          return true;
        },
        []() {},
        RestartPolicy::always};

    assert(component.valid() == false);
  }

  static void test_supervised_component_invalid_without_start_callback()
  {
    SupervisedComponent component{
        "missing-start",
        {},
        []() {},
        RestartPolicy::always};

    assert(component.valid() == false);
  }

  static void test_supervised_component_invalid_without_stop_callback()
  {
    SupervisedComponent component{
        "missing-stop",
        []()
        {
          return true;
        },
        {},
        RestartPolicy::always};

    assert(component.valid() == false);
  }

  static void test_supervisor_type_traits()
  {
    static_assert(std::is_default_constructible_v<Supervisor>);

    static_assert(!std::is_copy_constructible_v<Supervisor>);
    static_assert(!std::is_copy_assignable_v<Supervisor>);

    static_assert(!std::is_move_constructible_v<Supervisor>);
    static_assert(!std::is_move_assignable_v<Supervisor>);

    static_assert(std::is_destructible_v<Supervisor>);

    static_assert(std::is_default_constructible_v<SupervisedComponent>);
    static_assert(std::is_copy_constructible_v<SupervisedComponent>);
    static_assert(std::is_copy_assignable_v<SupervisedComponent>);
    static_assert(std::is_move_constructible_v<SupervisedComponent>);
    static_assert(std::is_move_assignable_v<SupervisedComponent>);
    static_assert(std::is_destructible_v<SupervisedComponent>);
  }

  static void test_default_supervisor_is_empty()
  {
    Supervisor supervisor;

    assert(supervisor.empty());
    assert(supervisor.size() == 0u);

    assert(!supervisor.state_of("missing").has_value());
    assert(!supervisor.restart_count_of("missing").has_value());

    assert(supervisor.start_all() == true);

    supervisor.stop_all();

    assert(supervisor.empty());
    assert(supervisor.size() == 0u);
  }

  static void test_add_valid_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    const bool added = supervisor.add(
        make_component(
            "cache",
            start_count,
            stop_count));

    assert(added);
    assert(!supervisor.empty());
    assert(supervisor.size() == 1u);

    auto state = supervisor.state_of("cache");
    auto restart_count = supervisor.restart_count_of("cache");

    assert(state.has_value());
    assert(*state == SupervisedState::stopped);

    assert(restart_count.has_value());
    assert(*restart_count == 0u);

    assert(start_count == 0);
    assert(stop_count == 0);
  }

  static void test_add_rejects_invalid_component()
  {
    Supervisor supervisor;

    SupervisedComponent invalid;

    const bool added = supervisor.add(std::move(invalid));

    assert(!added);
    assert(supervisor.empty());
    assert(supervisor.size() == 0u);
  }

  static void test_add_rejects_duplicate_name()
  {
    Supervisor supervisor;

    int first_start_count = 0;
    int first_stop_count = 0;
    int second_start_count = 0;
    int second_stop_count = 0;

    const bool first_added = supervisor.add(
        make_component(
            "service",
            first_start_count,
            first_stop_count));

    const bool second_added = supervisor.add(
        make_component(
            "service",
            second_start_count,
            second_stop_count));

    assert(first_added);
    assert(!second_added);
    assert(supervisor.size() == 1u);

    assert(supervisor.start_all() == true);
    supervisor.stop_all();

    assert(first_start_count == 1);
    assert(first_stop_count == 1);

    assert(second_start_count == 0);
    assert(second_stop_count == 0);
  }

  static void test_start_all_starts_components_in_registration_order()
  {
    Supervisor supervisor;

    std::vector<std::string> events;

    SupervisedComponent first{
        "first",
        [&events]()
        {
          events.push_back("start:first");
          return true;
        },
        [&events]()
        {
          events.push_back("stop:first");
        }};

    SupervisedComponent second{
        "second",
        [&events]()
        {
          events.push_back("start:second");
          return true;
        },
        [&events]()
        {
          events.push_back("stop:second");
        }};

    assert(supervisor.add(std::move(first)));
    assert(supervisor.add(std::move(second)));

    const bool ok = supervisor.start_all();

    assert(ok);
    assert(events.size() == 2u);
    assert(events[0] == "start:first");
    assert(events[1] == "start:second");

    assert(supervisor.state_of("first") == SupervisedState::running);
    assert(supervisor.state_of("second") == SupervisedState::running);

    supervisor.stop_all();
  }

  static void test_stop_all_stops_running_components_in_reverse_order()
  {
    Supervisor supervisor;

    std::vector<std::string> events;

    SupervisedComponent first{
        "first",
        [&events]()
        {
          events.push_back("start:first");
          return true;
        },
        [&events]()
        {
          events.push_back("stop:first");
        }};

    SupervisedComponent second{
        "second",
        [&events]()
        {
          events.push_back("start:second");
          return true;
        },
        [&events]()
        {
          events.push_back("stop:second");
        }};

    assert(supervisor.add(std::move(first)));
    assert(supervisor.add(std::move(second)));

    assert(supervisor.start_all());

    supervisor.stop_all();

    assert(events.size() == 4u);
    assert(events[0] == "start:first");
    assert(events[1] == "start:second");
    assert(events[2] == "stop:second");
    assert(events[3] == "stop:first");

    assert(supervisor.state_of("first") == SupervisedState::stopped);
    assert(supervisor.state_of("second") == SupervisedState::stopped);
  }

  static void test_start_all_is_idempotent_for_running_components()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "worker",
            start_count,
            stop_count)));

    assert(supervisor.start_all());
    assert(supervisor.start_all());
    assert(supervisor.start_all());

    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.state_of("worker") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 1);
  }

  static void test_stop_all_is_idempotent()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "worker",
            start_count,
            stop_count)));

    assert(supervisor.start_all());

    supervisor.stop_all();
    supervisor.stop_all();
    supervisor.stop_all();

    assert(start_count == 1);
    assert(stop_count == 1);
    assert(supervisor.state_of("worker") == SupervisedState::stopped);
  }

  static void test_start_all_marks_failed_component_when_start_returns_false()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_failing_component(
            "bad",
            start_count,
            stop_count)));

    const bool ok = supervisor.start_all();

    assert(!ok);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.state_of("bad") == SupervisedState::failed);

    supervisor.stop_all();

    assert(stop_count == 0);
  }

  static void test_start_all_marks_failed_component_when_start_throws()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_throwing_start_component(
            "throwing",
            start_count,
            stop_count)));

    const bool ok = supervisor.start_all();

    assert(!ok);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.state_of("throwing") == SupervisedState::failed);

    supervisor.stop_all();

    assert(stop_count == 0);
  }

  static void test_start_all_continues_after_failure()
  {
    Supervisor supervisor;

    int good_start_count = 0;
    int good_stop_count = 0;
    int bad_start_count = 0;
    int bad_stop_count = 0;
    int after_start_count = 0;
    int after_stop_count = 0;

    assert(supervisor.add(
        make_component(
            "good",
            good_start_count,
            good_stop_count)));

    assert(supervisor.add(
        make_failing_component(
            "bad",
            bad_start_count,
            bad_stop_count)));

    assert(supervisor.add(
        make_component(
            "after",
            after_start_count,
            after_stop_count)));

    const bool ok = supervisor.start_all();

    assert(!ok);

    assert(good_start_count == 1);
    assert(bad_start_count == 1);
    assert(after_start_count == 1);

    assert(supervisor.state_of("good") == SupervisedState::running);
    assert(supervisor.state_of("bad") == SupervisedState::failed);
    assert(supervisor.state_of("after") == SupervisedState::running);

    supervisor.stop_all();

    assert(good_stop_count == 1);
    assert(bad_stop_count == 0);
    assert(after_stop_count == 1);
  }

  static void test_stop_all_swallows_stop_exceptions()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_throwing_stop_component(
            "throw-stop",
            start_count,
            stop_count)));

    assert(supervisor.start_all());

    supervisor.stop_all();

    assert(start_count == 1);
    assert(stop_count == 1);
    assert(supervisor.state_of("throw-stop") == SupervisedState::stopped);
  }

  static void test_mark_failed_existing_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "api",
            start_count,
            stop_count)));

    assert(supervisor.start_all());

    assert(supervisor.state_of("api") == SupervisedState::running);

    const bool marked = supervisor.mark_failed("api");

    assert(marked);
    assert(supervisor.state_of("api") == SupervisedState::failed);

    supervisor.stop_all();

    assert(stop_count == 0);
  }

  static void test_mark_failed_missing_component_returns_false()
  {
    Supervisor supervisor;

    assert(!supervisor.mark_failed("missing"));
    assert(!supervisor.state_of("missing").has_value());
  }

  static void test_restart_missing_component_returns_false()
  {
    Supervisor supervisor;

    assert(!supervisor.restart("missing"));
  }

  static void test_restart_never_policy_is_denied()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "never",
            start_count,
            stop_count,
            RestartPolicy::never)));

    assert(supervisor.start_all());

    assert(start_count == 1);
    assert(supervisor.state_of("never") == SupervisedState::running);

    const bool restarted = supervisor.restart("never");

    assert(!restarted);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("never") == 0u);
    assert(supervisor.state_of("never") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 1);
  }

  static void test_restart_always_policy_restarts_running_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "always",
            start_count,
            stop_count,
            RestartPolicy::always)));

    assert(supervisor.start_all());

    assert(start_count == 1);
    assert(stop_count == 0);

    const bool restarted = supervisor.restart("always");

    assert(restarted);
    assert(start_count == 2);
    assert(stop_count == 1);
    assert(supervisor.restart_count_of("always") == 1u);
    assert(supervisor.state_of("always") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 2);
  }

  static void test_restart_always_policy_restarts_stopped_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "always",
            start_count,
            stop_count,
            RestartPolicy::always)));

    const bool restarted = supervisor.restart("always");

    assert(restarted);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("always") == 1u);
    assert(supervisor.state_of("always") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 1);
  }

  static void test_restart_on_failure_policy_denies_running_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "on-failure",
            start_count,
            stop_count,
            RestartPolicy::on_failure)));

    assert(supervisor.start_all());

    const bool restarted = supervisor.restart("on-failure");

    assert(!restarted);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("on-failure") == 0u);
    assert(supervisor.state_of("on-failure") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 1);
  }

  static void test_restart_on_failure_policy_denies_stopped_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "on-failure",
            start_count,
            stop_count,
            RestartPolicy::on_failure)));

    const bool restarted = supervisor.restart("on-failure");

    assert(!restarted);
    assert(start_count == 0);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("on-failure") == 0u);
    assert(supervisor.state_of("on-failure") == SupervisedState::stopped);
  }

  static void test_restart_on_failure_policy_restarts_failed_component()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "on-failure",
            start_count,
            stop_count,
            RestartPolicy::on_failure)));

    assert(supervisor.start_all());

    assert(supervisor.mark_failed("on-failure"));
    assert(supervisor.state_of("on-failure") == SupervisedState::failed);

    const bool restarted = supervisor.restart("on-failure");

    assert(restarted);
    assert(start_count == 2);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("on-failure") == 1u);
    assert(supervisor.state_of("on-failure") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 1);
  }

  static void test_restart_failed_start_does_not_increment_restart_count()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_failing_component(
            "bad",
            start_count,
            stop_count,
            RestartPolicy::always)));

    const bool restarted = supervisor.restart("bad");

    assert(!restarted);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("bad") == 0u);
    assert(supervisor.state_of("bad") == SupervisedState::failed);
  }

  static void test_restart_throwing_start_does_not_increment_restart_count()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_throwing_start_component(
            "throwing",
            start_count,
            stop_count,
            RestartPolicy::always)));

    const bool restarted = supervisor.restart("throwing");

    assert(!restarted);
    assert(start_count == 1);
    assert(stop_count == 0);
    assert(supervisor.restart_count_of("throwing") == 0u);
    assert(supervisor.state_of("throwing") == SupervisedState::failed);
  }

  static void test_restart_count_accumulates()
  {
    Supervisor supervisor;

    int start_count = 0;
    int stop_count = 0;

    assert(supervisor.add(
        make_component(
            "always",
            start_count,
            stop_count,
            RestartPolicy::always)));

    assert(supervisor.restart("always"));
    assert(supervisor.restart("always"));
    assert(supervisor.restart("always"));

    assert(start_count == 3);
    assert(stop_count == 2);
    assert(supervisor.restart_count_of("always") == 3u);
    assert(supervisor.state_of("always") == SupervisedState::running);

    supervisor.stop_all();

    assert(stop_count == 3);
  }

  static void test_state_of_missing_component_returns_nullopt()
  {
    Supervisor supervisor;

    assert(!supervisor.state_of("missing").has_value());
  }

  static void test_restart_count_of_missing_component_returns_nullopt()
  {
    Supervisor supervisor;

    assert(!supervisor.restart_count_of("missing").has_value());
  }

  static void test_destructor_stops_running_components()
  {
    int start_count = 0;
    int stop_count = 0;

    {
      Supervisor supervisor;

      assert(supervisor.add(
          make_component(
              "service",
              start_count,
              stop_count)));

      assert(supervisor.start_all());

      assert(start_count == 1);
      assert(stop_count == 0);
    }

    assert(start_count == 1);
    assert(stop_count == 1);
  }

  static void test_destructor_ignores_failed_components()
  {
    int start_count = 0;
    int stop_count = 0;

    {
      Supervisor supervisor;

      assert(supervisor.add(
          make_failing_component(
              "bad",
              start_count,
              stop_count)));

      assert(!supervisor.start_all());

      assert(start_count == 1);
      assert(stop_count == 0);
      assert(supervisor.state_of("bad") == SupervisedState::failed);
    }

    assert(start_count == 1);
    assert(stop_count == 0);
  }

  static void test_many_components_can_be_started_and_stopped()
  {
    Supervisor supervisor;

    constexpr int count = 50;

    std::vector<int> starts(static_cast<std::size_t>(count), 0);
    std::vector<int> stops(static_cast<std::size_t>(count), 0);

    for (int i = 0; i < count; ++i)
    {
      assert(supervisor.add(
          make_component(
              "component-" + std::to_string(i),
              starts[static_cast<std::size_t>(i)],
              stops[static_cast<std::size_t>(i)])));
    }

    assert(supervisor.size() == static_cast<std::size_t>(count));
    assert(supervisor.start_all());

    for (int i = 0; i < count; ++i)
    {
      assert(starts[static_cast<std::size_t>(i)] == 1);
      assert(stops[static_cast<std::size_t>(i)] == 0);
      assert(supervisor.state_of("component-" + std::to_string(i)) == SupervisedState::running);
    }

    supervisor.stop_all();

    for (int i = 0; i < count; ++i)
    {
      assert(starts[static_cast<std::size_t>(i)] == 1);
      assert(stops[static_cast<std::size_t>(i)] == 1);
      assert(supervisor.state_of("component-" + std::to_string(i)) == SupervisedState::stopped);
    }
  }

} // namespace

int main()
{
  test_restart_policy_numeric_values_are_stable();
  test_supervised_state_numeric_values_are_stable();

  test_supervised_component_default_state();
  test_supervised_component_valid_when_name_and_callbacks_exist();
  test_supervised_component_invalid_without_name();
  test_supervised_component_invalid_without_start_callback();
  test_supervised_component_invalid_without_stop_callback();

  test_supervisor_type_traits();
  test_default_supervisor_is_empty();

  test_add_valid_component();
  test_add_rejects_invalid_component();
  test_add_rejects_duplicate_name();

  test_start_all_starts_components_in_registration_order();
  test_stop_all_stops_running_components_in_reverse_order();

  test_start_all_is_idempotent_for_running_components();
  test_stop_all_is_idempotent();

  test_start_all_marks_failed_component_when_start_returns_false();
  test_start_all_marks_failed_component_when_start_throws();
  test_start_all_continues_after_failure();

  test_stop_all_swallows_stop_exceptions();

  test_mark_failed_existing_component();
  test_mark_failed_missing_component_returns_false();

  test_restart_missing_component_returns_false();
  test_restart_never_policy_is_denied();

  test_restart_always_policy_restarts_running_component();
  test_restart_always_policy_restarts_stopped_component();

  test_restart_on_failure_policy_denies_running_component();
  test_restart_on_failure_policy_denies_stopped_component();
  test_restart_on_failure_policy_restarts_failed_component();

  test_restart_failed_start_does_not_increment_restart_count();
  test_restart_throwing_start_does_not_increment_restart_count();
  test_restart_count_accumulates();

  test_state_of_missing_component_returns_nullopt();
  test_restart_count_of_missing_component_returns_nullopt();

  test_destructor_stops_running_components();
  test_destructor_ignores_failed_components();

  test_many_components_can_be_started_and_stopped();

  return 0;
}
