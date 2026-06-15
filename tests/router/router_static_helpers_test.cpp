/**
 *
 * @file router_static_helpers_test.cpp
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
#include <memory>
#include <string>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/Router.hpp>

namespace
{
  class DummyHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const vix::http::Request &,
        vix::http::Response &) override
    {
      co_return;
    }
  };

  static std::shared_ptr<vix::http::IRequestHandler> make_handler()
  {
    return std::make_shared<DummyHandler>();
  }

  static void test_strip_query_empty_target_becomes_root()
  {
    assert(vix::router::Router::strip_query("") == "/");
  }

  static void test_strip_query_root_stays_root()
  {
    assert(vix::router::Router::strip_query("/") == "/");
    assert(vix::router::Router::strip_query("/?debug=1") == "/");
  }

  static void test_strip_query_adds_leading_slash()
  {
    assert(vix::router::Router::strip_query("users") == "/users");
    assert(vix::router::Router::strip_query("users/42") == "/users/42");
    assert(vix::router::Router::strip_query("users/42?active=1") == "/users/42");
  }

  static void test_strip_query_removes_query_string()
  {
    assert(vix::router::Router::strip_query("/users?active=1") == "/users");
    assert(vix::router::Router::strip_query("/users/42?active=1&page=2") == "/users/42");
    assert(vix::router::Router::strip_query("/search?q=a?b") == "/search");
  }

  static void test_strip_query_removes_trailing_slash_except_root()
  {
    assert(vix::router::Router::strip_query("/") == "/");
    assert(vix::router::Router::strip_query("/users/") == "/users");
    assert(vix::router::Router::strip_query("/users/42/") == "/users/42");
    assert(vix::router::Router::strip_query("/users/42/?active=1") == "/users/42");
  }

  static void test_has_route_on_empty_router()
  {
    vix::router::Router router;

    assert(!router.has_route("GET", "/"));
    assert(!router.has_route("GET", "/users"));
    assert(!router.has_route("POST", "/users"));
  }

  static void test_has_route_normalizes_method_to_uppercase()
  {
    vix::router::Router router;

    router.add_route("get", "/users", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.has_route("get", "/users"));
    assert(router.has_route("GeT", "/users"));

    assert(!router.has_route("POST", "/users"));
  }

  static void test_has_route_normalizes_path_leading_slash()
  {
    vix::router::Router router;

    router.add_route("GET", "users", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.has_route("GET", "users"));
  }

  static void test_has_route_normalizes_trailing_slash()
  {
    vix::router::Router router;

    router.add_route("GET", "/users/", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.has_route("GET", "/users/"));
  }

  static void test_has_route_strips_query_string()
  {
    vix::router::Router router;

    router.add_route("GET", "/users", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.has_route("GET", "/users?active=1"));
    assert(router.has_route("GET", "/users?page=2&limit=10"));

    assert(!router.has_route("GET", "/projects?active=1"));
  }

  static void test_has_route_for_root_path()
  {
    vix::router::Router router;

    router.add_route("GET", "/", make_handler());

    assert(router.has_route("GET", "/"));
    assert(router.has_route("GET", ""));
    assert(router.has_route("GET", "/?debug=1"));

    assert(!router.has_route("POST", "/"));
  }

  static void test_has_route_for_static_nested_path()
  {
    vix::router::Router router;

    router.add_route("GET", "/api/users/list", make_handler());

    assert(router.has_route("GET", "/api/users/list"));
    assert(router.has_route("GET", "api/users/list"));
    assert(router.has_route("GET", "/api/users/list/"));
    assert(router.has_route("GET", "/api/users/list?page=1"));

    assert(!router.has_route("GET", "/api/users"));
    assert(!router.has_route("GET", "/api/users/list/details"));
  }

  static void test_has_route_for_param_path()
  {
    vix::router::Router router;

    router.add_route("GET", "/users/{id}", make_handler());

    assert(router.has_route("GET", "/users/42"));
    assert(router.has_route("GET", "/users/gaspard"));
    assert(router.has_route("GET", "/users/vix-core"));
    assert(router.has_route("GET", "/users/42?active=1"));

    assert(!router.has_route("GET", "/users"));
    assert(!router.has_route("GET", "/users/42/profile"));
  }

  static void test_has_route_static_segment_wins_when_registered()
  {
    vix::router::Router router;

    router.add_route("GET", "/users/{id}", make_handler());
    router.add_route("GET", "/users/me", make_handler());

    assert(router.has_route("GET", "/users/42"));
    assert(router.has_route("GET", "/users/me"));
  }

  static void test_has_route_distinguishes_methods()
  {
    vix::router::Router router;

    router.add_route("GET", "/users", make_handler());
    router.add_route("POST", "/users", make_handler());
    router.add_route("DELETE", "/users/{id}", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.has_route("POST", "/users"));
    assert(router.has_route("DELETE", "/users/42"));

    assert(!router.has_route("PUT", "/users"));
    assert(!router.has_route("PATCH", "/users/42"));
  }

} // namespace

int main()
{
  test_strip_query_empty_target_becomes_root();
  test_strip_query_root_stays_root();
  test_strip_query_adds_leading_slash();
  test_strip_query_removes_query_string();
  test_strip_query_removes_trailing_slash_except_root();

  test_has_route_on_empty_router();
  test_has_route_normalizes_method_to_uppercase();
  test_has_route_normalizes_path_leading_slash();
  test_has_route_normalizes_trailing_slash();
  test_has_route_strips_query_string();
  test_has_route_for_root_path();
  test_has_route_for_static_nested_path();
  test_has_route_for_param_path();
  test_has_route_static_segment_wins_when_registered();
  test_has_route_distinguishes_methods();

  return 0;
}
