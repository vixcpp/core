/**
 *
 * @file router_registration_test.cpp
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
#include <vix/http/RequestState.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/RouteDoc.hpp>
#include <vix/router/RouteOptions.hpp>
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

  static vix::http::Request make_request(
      std::string method,
      std::string target)
  {
    return vix::http::Request{
        std::move(method),
        std::move(target),
        vix::http::Request::HeaderMap{},
        std::string{},
        vix::http::Request::ParamMap{},
        std::make_shared<vix::http::RequestState>()};
  }

  static void test_new_router_has_no_registered_routes()
  {
    vix::router::Router router;

    assert(router.routes().empty());

    assert(!router.has_route("GET", "/"));
    assert(!router.has_route("GET", "/users"));
  }

  static void test_add_simple_route_registers_record()
  {
    vix::router::Router router;

    router.add_route("GET", "/users", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.routes().size() == 1);

    const auto &record = router.routes()[0];

    assert(record.method == "GET");
    assert(record.path == "/users");
    assert(record.heavy == false);
    assert(record.doc.empty());
  }

  static void test_add_route_normalizes_method()
  {
    vix::router::Router router;

    router.add_route("post", "/users", make_handler());

    assert(router.has_route("POST", "/users"));
    assert(router.has_route("post", "/users"));
    assert(router.has_route("PoSt", "/users"));

    assert(router.routes().size() == 1);
    assert(router.routes()[0].method == "POST");
  }

  static void test_add_route_normalizes_path()
  {
    vix::router::Router router;

    router.add_route("GET", "users/", make_handler());

    assert(router.has_route("GET", "/users"));
    assert(router.has_route("GET", "users"));
    assert(router.has_route("GET", "/users/"));

    assert(router.routes().size() == 1);
    assert(router.routes()[0].path == "/users");
  }

  static void test_add_root_route()
  {
    vix::router::Router router;

    router.add_route("GET", "", make_handler());

    assert(router.has_route("GET", "/"));
    assert(router.has_route("GET", ""));
    assert(router.has_route("GET", "/?debug=1"));

    assert(router.routes().size() == 1);
    assert(router.routes()[0].method == "GET");
    assert(router.routes()[0].path == "/");
  }

  static void test_add_nested_static_route()
  {
    vix::router::Router router;

    router.add_route("GET", "/api/v1/users/list", make_handler());

    assert(router.has_route("GET", "/api/v1/users/list"));
    assert(router.has_route("GET", "api/v1/users/list"));
    assert(router.has_route("GET", "/api/v1/users/list/"));
    assert(router.has_route("GET", "/api/v1/users/list?page=1"));

    assert(!router.has_route("GET", "/api/v1/users"));
    assert(!router.has_route("GET", "/api/v1/users/list/details"));

    assert(router.routes().size() == 1);
    assert(router.routes()[0].path == "/api/v1/users/list");
  }

  static void test_add_param_route()
  {
    vix::router::Router router;

    router.add_route("GET", "/users/{id}", make_handler());

    assert(router.has_route("GET", "/users/42"));
    assert(router.has_route("GET", "/users/gaspard"));
    assert(router.has_route("GET", "/users/vix-core"));
    assert(router.has_route("GET", "/users/42?active=1"));

    assert(!router.has_route("GET", "/users"));
    assert(!router.has_route("GET", "/users/42/profile"));

    assert(router.routes().size() == 1);
    assert(router.routes()[0].method == "GET");
    assert(router.routes()[0].path == "/users/{id}");
    assert(router.routes()[0].heavy == false);
  }

  static void test_add_multiple_param_route()
  {
    vix::router::Router router;

    router.add_route("GET", "/posts/{post_id}/comments/{comment_id}", make_handler());

    assert(router.has_route("GET", "/posts/10/comments/99"));
    assert(router.has_route("GET", "/posts/abc/comments/xyz"));

    assert(!router.has_route("GET", "/posts/10/comments"));
    assert(!router.has_route("GET", "/posts/10/comments/99/edit"));

    assert(router.routes().size() == 1);
    assert(router.routes()[0].path == "/posts/{post_id}/comments/{comment_id}");
  }

  static void test_add_same_path_with_different_methods()
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

    assert(router.routes().size() == 3);

    assert(router.routes()[0].method == "GET");
    assert(router.routes()[0].path == "/users");

    assert(router.routes()[1].method == "POST");
    assert(router.routes()[1].path == "/users");

    assert(router.routes()[2].method == "DELETE");
    assert(router.routes()[2].path == "/users/{id}");
  }

  static void test_registration_order_is_preserved()
  {
    vix::router::Router router;

    router.add_route("GET", "/health", make_handler());
    router.add_route("GET", "/users", make_handler());
    router.add_route("POST", "/users", make_handler());
    router.add_route("GET", "/projects/{slug}", make_handler());

    assert(router.routes().size() == 4);

    assert(router.routes()[0].method == "GET");
    assert(router.routes()[0].path == "/health");

    assert(router.routes()[1].method == "GET");
    assert(router.routes()[1].path == "/users");

    assert(router.routes()[2].method == "POST");
    assert(router.routes()[2].path == "/users");

    assert(router.routes()[3].method == "GET");
    assert(router.routes()[3].path == "/projects/{slug}");
  }

  static void test_add_light_route_with_options()
  {
    vix::router::Router router;

    vix::router::RouteOptions options{};
    options.heavy = false;

    router.add_route("GET", "/light", make_handler(), options);

    assert(router.has_route("GET", "/light"));
    assert(router.routes().size() == 1);
    assert(router.routes()[0].heavy == false);
    assert(router.routes()[0].doc.empty());

    auto req = make_request("GET", "/light");

    assert(router.is_heavy(req) == false);
  }

  static void test_add_heavy_route_with_options()
  {
    vix::router::Router router;

    vix::router::RouteOptions options{
        .heavy = true,
    };

    router.add_route("GET", "/heavy", make_handler(), options);

    assert(router.has_route("GET", "/heavy"));
    assert(router.routes().size() == 1);
    assert(router.routes()[0].heavy == true);

    assert(!router.routes()[0].doc.empty());
    assert(router.routes()[0].doc.x["x-vix-heavy"].get<bool>() == true);

    auto req = make_request("GET", "/heavy");

    assert(router.is_heavy(req) == true);
  }

  static void test_is_heavy_returns_false_for_missing_route()
  {
    vix::router::Router router;

    router.add_route("GET", "/heavy", make_handler(), vix::router::RouteOptions{.heavy = true});

    auto req = make_request("GET", "/missing");

    assert(router.is_heavy(req) == false);
  }

  static void test_is_heavy_strips_query_from_request_target()
  {
    vix::router::Router router;

    router.add_route("GET", "/reports/{id}", make_handler(), vix::router::RouteOptions{.heavy = true});

    auto req = make_request("GET", "/reports/42?format=json");

    assert(router.is_heavy(req) == true);
  }

  static void test_static_route_heavy_flag_wins_over_param_route()
  {
    vix::router::Router router;

    router.add_route("GET", "/users/{id}", make_handler(), vix::router::RouteOptions{.heavy = true});
    router.add_route("GET", "/users/me", make_handler(), vix::router::RouteOptions{.heavy = false});

    auto param_req = make_request("GET", "/users/42");
    auto static_req = make_request("GET", "/users/me");

    assert(router.is_heavy(param_req) == true);
    assert(router.is_heavy(static_req) == false);

    assert(router.has_route("GET", "/users/42"));
    assert(router.has_route("GET", "/users/me"));
  }

  static void test_static_route_can_be_heavy_while_param_route_is_light()
  {
    vix::router::Router router;

    router.add_route("GET", "/users/{id}", make_handler(), vix::router::RouteOptions{.heavy = false});
    router.add_route("GET", "/users/me", make_handler(), vix::router::RouteOptions{.heavy = true});

    auto param_req = make_request("GET", "/users/42");
    auto static_req = make_request("GET", "/users/me");

    assert(router.is_heavy(param_req) == false);
    assert(router.is_heavy(static_req) == true);
  }

  static void test_add_route_with_documentation()
  {
    vix::router::Router router;

    vix::router::RouteDoc doc;

    doc.summary = "List users";
    doc.description = "Return all users.";
    doc.tags = {"users", "read"};
    doc.responses = {
        {"200",
         {
             {"description", "Users returned"},
         }},
    };

    router.add_route(
        "GET",
        "/users",
        make_handler(),
        vix::router::RouteOptions{},
        doc);

    assert(router.has_route("GET", "/users"));
    assert(router.routes().size() == 1);

    const auto &record = router.routes()[0];

    assert(record.method == "GET");
    assert(record.path == "/users");
    assert(record.heavy == false);

    assert(record.doc.summary == "List users");
    assert(record.doc.description == "Return all users.");
    assert(record.doc.tags.size() == 2);
    assert(record.doc.tags[0] == "users");
    assert(record.doc.tags[1] == "read");
    assert(record.doc.responses["200"]["description"].get<std::string>() == "Users returned");
  }

  static void test_add_heavy_route_with_documentation_adds_x_vix_heavy()
  {
    vix::router::Router router;

    vix::router::RouteDoc doc;

    doc.summary = "Generate report";
    doc.tags = {"reports"};
    doc.x["x-custom"] = "kept";

    router.add_route(
        "POST",
        "/reports/{id}/generate",
        make_handler(),
        vix::router::RouteOptions{.heavy = true},
        doc);

    assert(router.has_route("POST", "/reports/42/generate"));
    assert(router.routes().size() == 1);

    const auto &record = router.routes()[0];

    assert(record.method == "POST");
    assert(record.path == "/reports/{id}/generate");
    assert(record.heavy == true);

    assert(record.doc.summary == "Generate report");
    assert(record.doc.tags.size() == 1);
    assert(record.doc.tags[0] == "reports");

    assert(record.doc.x["x-custom"].get<std::string>() == "kept");
    assert(record.doc.x["x-vix-heavy"].get<bool>() == true);
  }

  static void test_re_registering_same_route_replaces_handler_but_keeps_records()
  {
    vix::router::Router router;

    router.add_route("GET", "/users", make_handler());
    router.add_route("GET", "/users", make_handler(), vix::router::RouteOptions{.heavy = true});

    assert(router.has_route("GET", "/users"));
    assert(router.routes().size() == 2);

    assert(router.routes()[0].method == "GET");
    assert(router.routes()[0].path == "/users");
    assert(router.routes()[0].heavy == false);

    assert(router.routes()[1].method == "GET");
    assert(router.routes()[1].path == "/users");
    assert(router.routes()[1].heavy == true);

    auto req = make_request("GET", "/users");

    assert(router.is_heavy(req) == true);
  }

} // namespace

int main()
{
  test_new_router_has_no_registered_routes();
  test_add_simple_route_registers_record();
  test_add_route_normalizes_method();
  test_add_route_normalizes_path();
  test_add_root_route();
  test_add_nested_static_route();
  test_add_param_route();
  test_add_multiple_param_route();
  test_add_same_path_with_different_methods();
  test_registration_order_is_preserved();
  test_add_light_route_with_options();
  test_add_heavy_route_with_options();
  test_is_heavy_returns_false_for_missing_route();
  test_is_heavy_strips_query_from_request_target();
  test_static_route_heavy_flag_wins_over_param_route();
  test_static_route_can_be_heavy_while_param_route_is_light();
  test_add_route_with_documentation();
  test_add_heavy_route_with_documentation_adds_x_vix_heavy();
  test_re_registering_same_route_replaces_handler_but_keeps_records();

  return 0;
}
