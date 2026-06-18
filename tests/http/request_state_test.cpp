/**
 *
 * @file request_state_test.cpp
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
#include <stdexcept>
#include <string>
#include <utility>

#include <vix/http/RequestState.hpp>

namespace
{
  struct UserId
  {
    explicit UserId(int v)
        : value(v)
    {
    }

    int value{};
  };

  struct TraceId
  {
    std::string value;
  };

  struct RequestCounter
  {
    int value{};
  };

  struct ConstructedValue
  {
    explicit ConstructedValue(std::string v)
        : value(std::move(v))
    {
    }

    std::string value;
  };

  static void test_empty_state()
  {
    vix::http::RequestState state;

    assert(!state.has<int>());
    assert(!state.has<std::string>());
    assert(state.try_get<int>() == nullptr);
    assert(state.try_get<std::string>() == nullptr);
  }

  static void test_emplace_constructs_and_returns_reference()
  {
    vix::http::RequestState state;

    UserId &user = state.emplace<UserId>(42);

    assert(state.has<UserId>());
    assert(user.value == 42);
    assert(state.get<UserId>().value == 42);

    user.value = 100;

    assert(state.get<UserId>().value == 100);
  }

  static void test_set_stores_value()
  {
    vix::http::RequestState state;

    state.set<std::string>("vix");
    state.set<int>(123);

    assert(state.has<std::string>());
    assert(state.has<int>());

    assert(state.get<std::string>() == "vix");
    assert(state.get<int>() == 123);
  }

  static void test_set_overwrites_existing_value_of_same_type()
  {
    vix::http::RequestState state;

    state.set<int>(1);
    assert(state.get<int>() == 1);

    state.set<int>(2);
    assert(state.get<int>() == 2);

    state.emplace<int>(3);
    assert(state.get<int>() == 3);
  }

  static void test_different_types_are_stored_independently()
  {
    vix::http::RequestState state;

    state.set<UserId>(UserId{7});
    state.set<TraceId>(TraceId{"trace-001"});
    state.set<RequestCounter>(RequestCounter{9});

    assert(state.has<UserId>());
    assert(state.has<TraceId>());
    assert(state.has<RequestCounter>());

    assert(state.get<UserId>().value == 7);
    assert(state.get<TraceId>().value == "trace-001");
    assert(state.get<RequestCounter>().value == 9);
  }

  static void test_get_returns_mutable_reference()
  {
    vix::http::RequestState state;

    state.set<RequestCounter>(RequestCounter{1});

    RequestCounter &counter = state.get<RequestCounter>();
    counter.value += 10;

    assert(state.get<RequestCounter>().value == 11);
  }

  static void test_const_get_returns_const_reference()
  {
    vix::http::RequestState state;
    state.set<TraceId>(TraceId{"trace-const"});

    const vix::http::RequestState &const_state = state;

    const TraceId &trace = const_state.get<TraceId>();

    assert(trace.value == "trace-const");
  }

  static void test_try_get_returns_pointer_when_present()
  {
    vix::http::RequestState state;

    state.set<std::string>("hello");

    std::string *value = state.try_get<std::string>();

    assert(value != nullptr);
    assert(*value == "hello");

    *value = "updated";

    assert(state.get<std::string>() == "updated");
  }

  static void test_try_get_returns_nullptr_when_missing()
  {
    vix::http::RequestState state;

    state.set<int>(42);

    assert(state.try_get<std::string>() == nullptr);
    assert(state.try_get<double>() == nullptr);
  }

  static void test_const_try_get_returns_const_pointer()
  {
    vix::http::RequestState state;

    state.set<std::string>("const-pointer");

    const vix::http::RequestState &const_state = state;

    const std::string *value = const_state.try_get<std::string>();

    assert(value != nullptr);
    assert(*value == "const-pointer");
    assert(const_state.try_get<int>() == nullptr);
  }

  static void test_get_throws_when_type_is_missing()
  {
    vix::http::RequestState state;

    bool threw = false;

    try
    {
      (void)state.get<std::string>();
    }
    catch (const std::runtime_error &e)
    {
      threw = true;

      const std::string message = e.what();
      assert(message.find("RequestState missing type:") != std::string::npos);
    }

    assert(threw);
  }

  static void test_const_get_throws_when_type_is_missing()
  {
    vix::http::RequestState state;

    const vix::http::RequestState &const_state = state;

    bool threw = false;

    try
    {
      (void)const_state.get<int>();
    }
    catch (const std::runtime_error &e)
    {
      threw = true;

      const std::string message = e.what();
      assert(message.find("RequestState missing type:") != std::string::npos);
    }

    assert(threw);
  }

  static void test_moved_request_state_preserves_values()
  {
    vix::http::RequestState state;

    state.set<int>(77);
    state.set<std::string>("moved");

    vix::http::RequestState moved = std::move(state);

    assert(moved.has<int>());
    assert(moved.has<std::string>());

    assert(moved.get<int>() == 77);
    assert(moved.get<std::string>() == "moved");
  }

  static void test_move_assignment_preserves_values()
  {
    vix::http::RequestState source;
    source.set<int>(88);
    source.set<std::string>("move-assigned");

    vix::http::RequestState target;
    target.set<double>(3.14);

    target = std::move(source);

    assert(target.has<int>());
    assert(target.has<std::string>());
    assert(!target.has<double>());

    assert(target.get<int>() == 88);
    assert(target.get<std::string>() == "move-assigned");
  }

  static void test_emplace_supports_constructed_copyable_values()
  {
    vix::http::RequestState state;

    state.emplace<ConstructedValue>("constructed");

    assert(state.has<ConstructedValue>());
    assert(state.get<ConstructedValue>().value == "constructed");

    state.get<ConstructedValue>().value = "updated";

    assert(state.get<ConstructedValue>().value == "updated");
  }

} // namespace

int main()
{
  test_empty_state();
  test_emplace_constructs_and_returns_reference();
  test_set_stores_value();
  test_set_overwrites_existing_value_of_same_type();
  test_different_types_are_stored_independently();
  test_get_returns_mutable_reference();
  test_const_get_returns_const_reference();
  test_try_get_returns_pointer_when_present();
  test_try_get_returns_nullptr_when_missing();
  test_const_try_get_returns_const_pointer();
  test_get_throws_when_type_is_missing();
  test_const_get_throws_when_type_is_missing();
  test_moved_request_state_preserves_values();
  test_move_assignment_preserves_values();
  test_emplace_supports_constructed_copyable_values();

  return 0;
}
