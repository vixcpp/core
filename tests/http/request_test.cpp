/**
 *
 * @file request_test.cpp
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
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include <vix/http/Request.hpp>
#include <vix/http/RequestState.hpp>
#include <vix/json/json.hpp>

namespace
{
  using Request = vix::http::Request;
  using HeaderMap = Request::HeaderMap;
  using ParamMap = Request::ParamMap;
  using StatePtr = Request::StatePtr;

  struct TraceId
  {
    std::string value;
  };

  struct UserId
  {
    int value{};
  };

  static StatePtr make_state()
  {
    return std::make_shared<vix::http::RequestState>();
  }

  static Request make_request(
      std::string method,
      std::string target,
      HeaderMap headers = HeaderMap{},
      std::string body = std::string{},
      ParamMap params = ParamMap{})
  {
    return Request{
        std::move(method),
        std::move(target),
        std::move(headers),
        std::move(body),
        std::move(params),
        make_state()};
  }

  static void test_default_request()
  {
    Request req;

    assert(req.method().empty());
    assert(req.target().empty());
    assert(req.path().empty());
    assert(req.query_string().empty());
    assert(req.body().empty());

    assert(req.headers().empty());
    assert(req.params().empty());
    assert(req.query().empty());

    assert(!req.has_header("Content-Type"));
    assert(req.header("Content-Type").empty());

    assert(!req.has_param("id"));
    assert(req.param("id").empty());
    assert(req.param("id", "fallback") == "fallback");

    assert(!req.has_query("page"));
    assert(req.query_value("page").empty());
    assert(req.query_value("page", "1") == "1");

    assert(req.has_state());
    assert(req.state_ptr() != nullptr);
  }

  static void test_constructed_request_splits_target()
  {
    Request req = make_request("GET", "/users/42?active=1&page=2");

    assert(req.method() == "GET");
    assert(req.target() == "/users/42?active=1&page=2");
    assert(req.path() == "/users/42");
    assert(req.query_string() == "active=1&page=2");

    assert(req.has_query("active"));
    assert(req.has_query("page"));
    assert(req.query_value("active") == "1");
    assert(req.query_value("page") == "2");
  }

  static void test_target_without_query()
  {
    Request req = make_request("GET", "/health");

    assert(req.target() == "/health");
    assert(req.path() == "/health");
    assert(req.query_string().empty());
    assert(req.query().empty());

    assert(!req.has_query("missing"));
    assert(req.query_value("missing", "fallback") == "fallback");
  }

  static void test_target_with_empty_query_string()
  {
    Request req = make_request("GET", "/search?");

    assert(req.target() == "/search?");
    assert(req.path() == "/search");
    assert(req.query_string().empty());
    assert(req.query().empty());
  }

  static void test_target_with_multiple_question_marks()
  {
    Request req = make_request("GET", "/search?q=a?b");

    assert(req.path() == "/search");
    assert(req.query_string() == "q=a?b");
    assert(req.query_value("q") == "a?b");
  }

  static void test_set_target_resets_query_cache()
  {
    Request req = make_request("GET", "/items?page=1");

    assert(req.query_value("page") == "1");

    req.set_target("/items?page=2&limit=10");

    assert(req.target() == "/items?page=2&limit=10");
    assert(req.path() == "/items");
    assert(req.query_string() == "page=2&limit=10");
    assert(req.query_value("page") == "2");
    assert(req.query_value("limit") == "10");
    assert(!req.has_query("missing"));
  }

  static void test_query_map_is_available_on_const_request()
  {
    const Request req = make_request("GET", "/users?role=admin");

    assert(req.has_query("role"));
    assert(req.query_value("role") == "admin");

    const auto &query = req.query();

    assert(query.at("role") == "admin");
  }

  static void test_headers()
  {
    Request req = make_request(
        "POST",
        "/users",
        HeaderMap{
            {"Content-Type", "application/json"},
            {"X-Request-Id", "req-001"},
        },
        "{}");

    assert(req.has_header("Content-Type"));
    assert(req.has_header("X-Request-Id"));

    assert(req.header("Content-Type") == "application/json");
    assert(req.header("X-Request-Id") == "req-001");
    assert(req.header("Missing").empty());

    req.set_header("X-Trace-Id", "trace-001");
    assert(req.has_header("X-Trace-Id"));
    assert(req.header("X-Trace-Id") == "trace-001");

    req.set_header("X-Trace-Id", "trace-002");
    assert(req.header("X-Trace-Id") == "trace-002");

    req.remove_header("X-Trace-Id");
    assert(!req.has_header("X-Trace-Id"));
    assert(req.header("X-Trace-Id").empty());

    req.set_headers(HeaderMap{
        {"Accept", "application/json"},
        {"Authorization", "Bearer token"},
    });

    assert(!req.has_header("Content-Type"));
    assert(!req.has_header("X-Request-Id"));
    assert(req.has_header("Accept"));
    assert(req.has_header("Authorization"));
    assert(req.header("Accept") == "application/json");
    assert(req.header("Authorization") == "Bearer token");
  }

  static void test_headers_are_case_sensitive()
  {
    Request req;

    req.set_header("Content-Type", "application/json");

    assert(req.has_header("Content-Type"));
    assert(!req.has_header("content-type"));
    assert(req.header("Content-Type") == "application/json");
    assert(req.header("content-type").empty());
  }

  static void test_params()
  {
    Request req = make_request(
        "GET",
        "/users/42",
        HeaderMap{},
        "",
        ParamMap{
            {"id", "42"},
            {"section", "profile"},
        });

    assert(req.has_param("id"));
    assert(req.has_param("section"));
    assert(req.param("id") == "42");
    assert(req.param("section") == "profile");
    assert(req.param("missing").empty());
    assert(req.param("missing", "fallback") == "fallback");

    req.set_params(ParamMap{
        {"slug", "vix-core"},
    });

    assert(!req.has_param("id"));
    assert(!req.has_param("section"));
    assert(req.has_param("slug"));
    assert(req.param("slug") == "vix-core");
  }

  static void test_with_params_returns_copy_with_replaced_params()
  {
    Request req = make_request(
        "GET",
        "/users/42",
        HeaderMap{{"X-Test", "yes"}},
        "body",
        ParamMap{{"id", "42"}});

    Request copy = req.with_params(ParamMap{
        {"project", "vix"},
        {"module", "core"},
    });

    assert(req.has_param("id"));
    assert(req.param("id") == "42");
    assert(!req.has_param("project"));

    assert(!copy.has_param("id"));
    assert(copy.has_param("project"));
    assert(copy.has_param("module"));
    assert(copy.param("project") == "vix");
    assert(copy.param("module") == "core");

    assert(copy.method() == req.method());
    assert(copy.target() == req.target());
    assert(copy.path() == req.path());
    assert(copy.body() == req.body());
    assert(copy.header("X-Test") == "yes");
  }

  static void test_body_and_json()
  {
    Request req = make_request(
        "POST",
        "/users",
        HeaderMap{{"Content-Type", "application/json"}},
        R"({"id":42,"name":"Gaspard","active":true})");

    assert(req.body() == R"({"id":42,"name":"Gaspard","active":true})");

    const auto &json = req.json();

    assert(json["id"].get<int>() == 42);
    assert(json["name"].get<std::string>() == "Gaspard");
    assert(json["active"].get<bool>() == true);

    auto parsed = req.json_as<vix::json::Json>();

    assert(parsed["id"].get<int>() == 42);
    assert(parsed["name"].get<std::string>() == "Gaspard");
    assert(parsed["active"].get<bool>() == true);
  }

  static void test_set_body_resets_json_cache()
  {
    Request req = make_request(
        "POST",
        "/payload",
        HeaderMap{},
        R"({"version":1})");

    assert(req.json()["version"].get<int>() == 1);

    req.set_body(R"({"version":2,"ok":true})");

    assert(req.body() == R"({"version":2,"ok":true})");
    assert(req.json()["version"].get<int>() == 2);
    assert(req.json()["ok"].get<bool>() == true);
  }

  static void test_empty_body_parses_as_null_json_value()
  {
    Request req;

    assert(req.body().empty());

    const auto &json = req.json();

    assert(json.is_null());
  }

  static void test_invalid_json_body_throws_when_json_is_requested()
  {
    Request req = make_request(
        "POST",
        "/payload",
        HeaderMap{{"Content-Type", "application/json"}},
        "{invalid-json");

    bool threw = false;

    try
    {
      (void)req.json();
    }
    catch (const nlohmann::json::exception &)
    {
      threw = true;
    }

    assert(threw);
  }

  static void test_method_can_be_updated()
  {
    Request req;

    assert(req.method().empty());

    req.set_method("PATCH");

    assert(req.method() == "PATCH");
  }

  static void test_request_state_helpers()
  {
    Request req;

    assert(req.has_state());
    assert(req.state_ptr() != nullptr);
    assert(!req.has_state_type<TraceId>());
    assert(req.try_state<TraceId>() == nullptr);

    req.emplace_state<TraceId>(TraceId{"trace-001"});

    assert(req.has_state_type<TraceId>());
    assert(req.state<TraceId>().value == "trace-001");
    assert(req.try_state<TraceId>() != nullptr);
    assert(req.try_state<TraceId>()->value == "trace-001");

    req.state<TraceId>().value = "trace-002";

    assert(req.state<TraceId>().value == "trace-002");

    req.set_state<UserId>(UserId{42});

    assert(req.has_state_type<UserId>());
    assert(req.state<UserId>().value == 42);
  }

  static void test_request_state_helpers_on_const_request()
  {
    Request req;

    req.set_state<TraceId>(TraceId{"const-trace"});

    const Request &const_req = req;

    assert(const_req.has_state());
    assert(const_req.has_state_type<TraceId>());
    assert(const_req.state<TraceId>().value == "const-trace");
    assert(const_req.try_state<TraceId>() != nullptr);
    assert(const_req.try_state<TraceId>()->value == "const-trace");
    assert(const_req.state_ptr() != nullptr);
  }

  static void test_custom_state_container_is_shared()
  {
    auto state = make_state();

    state->set<TraceId>(TraceId{"shared-trace"});

    Request req{
        "GET",
        "/shared",
        HeaderMap{},
        "",
        ParamMap{},
        state};

    assert(req.has_state());
    assert(req.state_ptr() == state);
    assert(req.has_state_type<TraceId>());
    assert(req.state<TraceId>().value == "shared-trace");

    req.state<TraceId>().value = "updated-shared-trace";

    assert(state->get<TraceId>().value == "updated-shared-trace");
  }

  static void test_copy_preserves_request_data_and_shares_state()
  {
    Request req = make_request(
        "POST",
        "/copy?id=1",
        HeaderMap{{"Content-Type", "application/json"}},
        R"({"ok":true})",
        ParamMap{{"id", "1"}});

    req.set_state<TraceId>(TraceId{"copy-trace"});

    Request copy = req;

    assert(copy.method() == "POST");
    assert(copy.target() == "/copy?id=1");
    assert(copy.path() == "/copy");
    assert(copy.query_value("id") == "1");
    assert(copy.header("Content-Type") == "application/json");
    assert(copy.body() == R"({"ok":true})");
    assert(copy.param("id") == "1");
    assert(copy.json()["ok"].get<bool>() == true);

    assert(copy.state_ptr() == req.state_ptr());
    assert(copy.state<TraceId>().value == "copy-trace");

    copy.state<TraceId>().value = "changed-from-copy";

    assert(req.state<TraceId>().value == "changed-from-copy");
  }

  static void test_move_preserves_request_data()
  {
    Request req = make_request(
        "DELETE",
        "/items/7?force=true",
        HeaderMap{{"X-Mode", "test"}},
        R"({"delete":true})",
        ParamMap{{"id", "7"}});

    req.set_state<UserId>(UserId{7});

    Request moved = std::move(req);

    assert(moved.method() == "DELETE");
    assert(moved.target() == "/items/7?force=true");
    assert(moved.path() == "/items/7");
    assert(moved.query_value("force") == "true");
    assert(moved.header("X-Mode") == "test");
    assert(moved.body() == R"({"delete":true})");
    assert(moved.param("id") == "7");
    assert(moved.json()["delete"].get<bool>() == true);
    assert(moved.state<UserId>().value == 7);
  }

} // namespace

int main()
{
  test_default_request();
  test_constructed_request_splits_target();
  test_target_without_query();
  test_target_with_empty_query_string();
  test_target_with_multiple_question_marks();
  test_set_target_resets_query_cache();
  test_query_map_is_available_on_const_request();
  test_headers();
  test_headers_are_case_sensitive();
  test_params();
  test_with_params_returns_copy_with_replaced_params();
  test_body_and_json();
  test_set_body_resets_json_cache();
  test_empty_body_parses_as_null_json_value();
  test_invalid_json_body_throws_when_json_is_requested();
  test_method_can_be_updated();
  test_request_state_helpers();
  test_request_state_helpers_on_const_request();
  test_custom_state_container_is_shared();
  test_copy_preserves_request_data_and_shares_state();
  test_move_preserves_request_data();

  return 0;
}
