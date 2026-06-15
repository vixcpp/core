/**
 *
 * @file response_test.cpp
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
#include <string>

#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/json.hpp>

namespace
{
  using Response = vix::http::Response;
  using HeaderMap = Response::HeaderMap;

  static bool contains(const std::string &text, const std::string &needle)
  {
    return text.find(needle) != std::string::npos;
  }

  static vix::json::Json parse_body(const Response &res)
  {
    return vix::json::loads(res.body());
  }

  static void assert_default_http_headers(const Response &res, std::string expected_length)
  {
    assert(res.header("Server") == "Vix.cpp");
    assert(res.has_header("Date"));
    assert(res.header("Content-Length") == expected_length);
    assert(res.header("Connection") == "keep-alive");
  }

  static void test_default_constructor()
  {
    Response res;

    assert(res.status() == vix::http::OK);
    assert(res.body().empty());
    assert(res.headers().empty());
    assert(!res.should_close());
    assert(res.reason().empty());
    assert(res.version() == "HTTP/1.1");
  }

  static void test_status_constructor()
  {
    Response res{vix::http::CREATED};

    assert(res.status() == vix::http::CREATED);
    assert(res.body().empty());
    assert(res.headers().empty());
  }

  static void test_status_and_body_constructor()
  {
    Response res{vix::http::ACCEPTED, "accepted"};

    assert(res.status() == vix::http::ACCEPTED);
    assert(res.body() == "accepted");
    assert(res.headers().empty());
  }

  static void test_set_status_and_body()
  {
    Response res;

    res.set_status(vix::http::NOT_FOUND);
    res.set_body("missing");

    assert(res.status() == vix::http::NOT_FOUND);
    assert(res.body() == "missing");

    res.body() += " resource";

    assert(res.body() == "missing resource");
  }

  static void test_headers()
  {
    Response res;

    assert(!res.has_header("Content-Type"));
    assert(res.header("Content-Type").empty());

    res.set_header("Content-Type", "text/plain; charset=utf-8");
    res.set_header("X-Test", "yes");

    assert(res.has_header("Content-Type"));
    assert(res.has_header("X-Test"));
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    assert(res.header("X-Test") == "yes");

    res.set_header("X-Test", "updated");
    assert(res.header("X-Test") == "updated");

    res.remove_header("X-Test");
    assert(!res.has_header("X-Test"));
    assert(res.header("X-Test").empty());

    res.set_headers(HeaderMap{
        {"Accept", "application/json"},
        {"Cache-Control", "no-store"},
    });

    assert(!res.has_header("Content-Type"));
    assert(res.has_header("Accept"));
    assert(res.has_header("Cache-Control"));
    assert(res.header("Accept") == "application/json");
    assert(res.header("Cache-Control") == "no-store");

    res.clear_headers();

    assert(res.headers().empty());
    assert(!res.has_header("Accept"));
    assert(!res.has_header("Cache-Control"));
  }

  static void test_headers_are_case_sensitive()
  {
    Response res;

    res.set_header("Content-Type", "application/json");

    assert(res.has_header("Content-Type"));
    assert(!res.has_header("content-type"));
    assert(res.header("Content-Type") == "application/json");
    assert(res.header("content-type").empty());
  }

  static void test_close_flag()
  {
    Response res;

    assert(!res.should_close());

    res.set_should_close(true);
    assert(res.should_close());

    res.set_should_close(false);
    assert(!res.should_close());
  }

  static void test_reason_phrase()
  {
    Response res{vix::http::OK, "hello"};

    assert(res.reason().empty());

    res.set_reason("Everything Fine");
    assert(res.reason() == "Everything Fine");

    std::string raw = res.to_http_string();
    assert(contains(raw, "HTTP/1.1 200 Everything Fine\r\n"));

    res.clear_reason();
    assert(res.reason().empty());

    raw = res.to_http_string();
    assert(contains(raw, "HTTP/1.1 200 OK\r\n"));
  }

  static void test_http_version()
  {
    Response res{vix::http::OK, "hello"};

    assert(res.version() == "HTTP/1.1");

    res.set_version("HTTP/1.0");

    assert(res.version() == "HTTP/1.0");

    const std::string raw = res.to_http_string();

    assert(contains(raw, "HTTP/1.0 200 OK\r\n"));
  }

  static void test_to_http_string_applies_default_headers_without_mutating_original()
  {
    Response res{vix::http::OK, "hello"};

    assert(res.headers().empty());

    const std::string raw = res.to_http_string();

    assert(contains(raw, "HTTP/1.1 200 OK\r\n"));
    assert(contains(raw, "Server: Vix.cpp\r\n"));
    assert(contains(raw, "Date: "));
    assert(contains(raw, "Content-Length: 5\r\n"));
    assert(contains(raw, "Connection: keep-alive\r\n"));
    assert(contains(raw, "\r\n\r\nhello"));

    assert(res.headers().empty());
  }

  static void test_to_http_string_uses_close_connection_when_requested()
  {
    Response res{vix::http::OK, "bye"};

    res.set_should_close(true);

    const std::string raw = res.to_http_string();

    assert(contains(raw, "Connection: close\r\n"));
    assert(contains(raw, "Content-Length: 3\r\n"));
  }

  static void test_to_http_string_preserves_custom_headers()
  {
    Response res{vix::http::CREATED, "created"};

    res.set_header("Content-Type", "text/plain");
    res.set_header("X-App", "vix");

    const std::string raw = res.to_http_string();

    assert(contains(raw, "HTTP/1.1 201 Created\r\n"));
    assert(contains(raw, "Content-Type: text/plain\r\n"));
    assert(contains(raw, "X-App: vix\r\n"));
    assert(contains(raw, "Content-Length: 7\r\n"));
    assert(contains(raw, "\r\n\r\ncreated"));
  }

  static void test_to_http_string_unknown_status()
  {
    Response res{299, "custom"};

    const std::string raw = res.to_http_string();

    assert(contains(raw, "HTTP/1.1 299 Unknown\r\n"));
    assert(contains(raw, "Content-Length: 6\r\n"));
  }

  static void test_common_headers_adds_server_and_date()
  {
    Response res;

    Response::common_headers(res);

    assert(res.header("Server") == "Vix.cpp");
    assert(res.has_header("Date"));
  }

  static void test_common_headers_does_not_override_existing_values()
  {
    Response res;

    res.set_header("Server", "CustomServer");
    res.set_header("Date", "Mon, 01 Jan 2024 00:00:00 GMT");

    Response::common_headers(res);

    assert(res.header("Server") == "CustomServer");
    assert(res.header("Date") == "Mon, 01 Jan 2024 00:00:00 GMT");
  }

  static void test_static_has_header()
  {
    Response res;

    assert(!Response::has_header(res, "X-Test"));

    res.set_header("X-Test", "yes");

    assert(Response::has_header(res, "X-Test"));
  }

  static void test_text_response()
  {
    Response res;

    Response::text_response(res, "hello", vix::http::CREATED);

    assert(res.status() == vix::http::CREATED);
    assert(res.body() == "hello");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    assert_default_http_headers(res, "5");
  }

  static void test_text_response_does_not_override_content_type()
  {
    Response res;

    res.set_header("Content-Type", "text/custom");

    Response::text_response(res, "hello", vix::http::OK);

    assert(res.status() == vix::http::OK);
    assert(res.body() == "hello");
    assert(res.header("Content-Type") == "text/custom");
    assert_default_http_headers(res, "5");
  }

  static void test_json_response_with_vix_json()
  {
    Response res;

    vix::json::Json payload{
        {"ok", true},
        {"runtime", "vix"},
        {"count", 3},
    };

    Response::json_response(res, payload, vix::http::ACCEPTED);

    assert(res.status() == vix::http::ACCEPTED);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert_default_http_headers(res, std::to_string(res.body().size()));

    const auto parsed = parse_body(res);

    assert(parsed["ok"].get<bool>() == true);
    assert(parsed["runtime"].get<std::string>() == "vix");
    assert(parsed["count"].get<int>() == 3);
  }

  static void test_json_response_does_not_override_content_type()
  {
    Response res;

    res.set_header("Content-Type", "application/vnd.api+json");

    Response::json_response(
        res,
        vix::json::Json{{"ok", true}},
        vix::http::OK);

    assert(res.header("Content-Type") == "application/vnd.api+json");

    const auto parsed = parse_body(res);

    assert(parsed["ok"].get<bool>() == true);
  }

  static void test_create_response()
  {
    Response res;

    Response::create_response(res, vix::http::BAD_REQUEST, "Invalid input");

    assert(res.status() == vix::http::BAD_REQUEST);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert_default_http_headers(res, std::to_string(res.body().size()));

    const auto parsed = parse_body(res);

    assert(parsed["message"].get<std::string>() == "Invalid input");
  }

  static void test_create_response_custom_content_type()
  {
    Response res;

    Response::create_response(
        res,
        vix::http::OK,
        "plain",
        "application/custom+json");

    assert(res.status() == vix::http::OK);
    assert(res.header("Content-Type") == "application/custom+json");

    const auto parsed = parse_body(res);

    assert(parsed["message"].get<std::string>() == "plain");
  }

  static void test_success_response()
  {
    Response res;

    Response::success_response(res, "done");

    assert(res.status() == vix::http::OK);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert_default_http_headers(res, std::to_string(res.body().size()));

    const auto parsed = parse_body(res);

    assert(parsed["message"].get<std::string>() == "done");
  }

  static void test_error_response()
  {
    Response res;

    Response::error_response(res, vix::http::NOT_FOUND, "missing");

    assert(res.status() == vix::http::NOT_FOUND);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert_default_http_headers(res, std::to_string(res.body().size()));

    const auto parsed = parse_body(res);

    assert(parsed["message"].get<std::string>() == "missing");
  }

  static void test_no_content_response()
  {
    Response res;

    Response::no_content_response(res);

    assert(res.status() == vix::http::NO_CONTENT);
    assert(res.body().empty());
    assert_default_http_headers(res, "0");
  }

  static void test_redirect_response()
  {
    Response res;

    Response::redirect_response(res, "/login");

    assert(res.status() == vix::http::FOUND);
    assert(res.header("Location") == "/login");
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert_default_http_headers(res, std::to_string(res.body().size()));

    const auto parsed = parse_body(res);

    assert(parsed["message"].get<std::string>() == "Redirecting to /login");
  }

  static void test_to_json_string_with_json()
  {
    const vix::json::Json payload{
        {"name", "Vix.cpp"},
        {"ok", true},
    };

    const std::string out = Response::to_json_string(payload);
    const auto parsed = vix::json::loads(out);

    assert(parsed["name"].get<std::string>() == "Vix.cpp");
    assert(parsed["ok"].get<bool>() == true);
  }

  static void test_to_json_string_with_serializable_value()
  {
    const std::string out = Response::to_json_string(42);
    const auto parsed = vix::json::loads(out);

    assert(parsed.get<int>() == 42);
  }

  static void test_copy_preserves_response_data()
  {
    Response res{vix::http::CREATED, "created"};

    res.set_header("X-Test", "yes");
    res.set_reason("Custom Created");
    res.set_version("HTTP/1.0");
    res.set_should_close(true);

    Response copy = res;

    assert(copy.status() == vix::http::CREATED);
    assert(copy.body() == "created");
    assert(copy.header("X-Test") == "yes");
    assert(copy.reason() == "Custom Created");
    assert(copy.version() == "HTTP/1.0");
    assert(copy.should_close());
  }

  static void test_move_preserves_response_data()
  {
    Response res{vix::http::ACCEPTED, "accepted"};

    res.set_header("X-Move", "yes");
    res.set_reason("Custom Accepted");
    res.set_should_close(true);

    Response moved = std::move(res);

    assert(moved.status() == vix::http::ACCEPTED);
    assert(moved.body() == "accepted");
    assert(moved.header("X-Move") == "yes");
    assert(moved.reason() == "Custom Accepted");
    assert(moved.should_close());
  }

} // namespace

int main()
{
  test_default_constructor();
  test_status_constructor();
  test_status_and_body_constructor();
  test_set_status_and_body();
  test_headers();
  test_headers_are_case_sensitive();
  test_close_flag();
  test_reason_phrase();
  test_http_version();
  test_to_http_string_applies_default_headers_without_mutating_original();
  test_to_http_string_uses_close_connection_when_requested();
  test_to_http_string_preserves_custom_headers();
  test_to_http_string_unknown_status();
  test_common_headers_adds_server_and_date();
  test_common_headers_does_not_override_existing_values();
  test_static_has_header();
  test_text_response();
  test_text_response_does_not_override_content_type();
  test_json_response_with_vix_json();
  test_json_response_does_not_override_content_type();
  test_create_response();
  test_create_response_custom_content_type();
  test_success_response();
  test_error_response();
  test_no_content_response();
  test_redirect_response();
  test_to_json_string_with_json();
  test_to_json_string_with_serializable_value();
  test_copy_preserves_response_data();
  test_move_preserves_response_data();

  return 0;
}
