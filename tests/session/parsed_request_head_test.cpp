/**
 *
 * @file parsed_request_head_test.cpp
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
#include <cstddef>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <vix/session/Session.hpp>

namespace
{
  using ParsedRequestHead = vix::session::ParsedRequestHead;

  static void test_default_parsed_request_head()
  {
    ParsedRequestHead head;

    assert(head.method.empty());
    assert(head.target.empty());
    assert(head.version == "HTTP/1.1");
    assert(head.headers.empty());
    assert(head.content_length == 0);
    assert(head.keep_alive == true);
  }

  static void test_set_request_line_fields()
  {
    ParsedRequestHead head;

    head.method = "GET";
    head.target = "/health";
    head.version = "HTTP/1.1";

    assert(head.method == "GET");
    assert(head.target == "/health");
    assert(head.version == "HTTP/1.1");
  }

  static void test_set_http_10_version()
  {
    ParsedRequestHead head;

    head.method = "GET";
    head.target = "/";
    head.version = "HTTP/1.0";
    head.keep_alive = false;

    assert(head.method == "GET");
    assert(head.target == "/");
    assert(head.version == "HTTP/1.0");
    assert(head.keep_alive == false);
  }

  static void test_headers_can_be_added()
  {
    ParsedRequestHead head;

    head.headers["Host"] = "localhost";
    head.headers["User-Agent"] = "vix-test";
    head.headers["Accept"] = "application/json";

    assert(head.headers.size() == 3);
    assert(head.headers.at("Host") == "localhost");
    assert(head.headers.at("User-Agent") == "vix-test");
    assert(head.headers.at("Accept") == "application/json");
  }

  static void test_headers_can_be_replaced()
  {
    ParsedRequestHead head;

    head.headers["Content-Type"] = "text/plain";
    assert(head.headers.at("Content-Type") == "text/plain");

    head.headers["Content-Type"] = "application/json";
    assert(head.headers.at("Content-Type") == "application/json");
  }

  static void test_headers_are_case_sensitive_storage()
  {
    ParsedRequestHead head;

    head.headers["Connection"] = "close";
    head.headers["connection"] = "keep-alive";

    assert(head.headers.size() == 2);
    assert(head.headers.at("Connection") == "close");
    assert(head.headers.at("connection") == "keep-alive");
  }

  static void test_content_length_defaults_and_can_be_set()
  {
    ParsedRequestHead head;

    assert(head.content_length == 0);

    head.content_length = 128;

    assert(head.content_length == 128);
  }

  static void test_content_length_large_value()
  {
    ParsedRequestHead head;

    head.content_length = static_cast<std::size_t>(10 * 1024 * 1024);

    assert(head.content_length == static_cast<std::size_t>(10 * 1024 * 1024));
  }

  static void test_keep_alive_can_be_disabled()
  {
    ParsedRequestHead head;

    assert(head.keep_alive == true);

    head.keep_alive = false;

    assert(head.keep_alive == false);
  }

  static void test_keep_alive_can_be_enabled_again()
  {
    ParsedRequestHead head;

    head.keep_alive = false;
    assert(head.keep_alive == false);

    head.keep_alive = true;
    assert(head.keep_alive == true);
  }

  static void test_aggregate_initialization_for_get_request()
  {
    ParsedRequestHead head{
        .method = "GET",
        .target = "/users",
        .version = "HTTP/1.1",
        .headers = {
            {"Host", "localhost"},
            {"Connection", "keep-alive"},
        },
        .content_length = 0,
        .keep_alive = true,
    };

    assert(head.method == "GET");
    assert(head.target == "/users");
    assert(head.version == "HTTP/1.1");
    assert(head.headers.size() == 2);
    assert(head.headers.at("Host") == "localhost");
    assert(head.headers.at("Connection") == "keep-alive");
    assert(head.content_length == 0);
    assert(head.keep_alive == true);
  }

  static void test_aggregate_initialization_for_post_request()
  {
    ParsedRequestHead head{
        .method = "POST",
        .target = "/api/users",
        .version = "HTTP/1.1",
        .headers = {
            {"Host", "localhost"},
            {"Content-Type", "application/json"},
            {"Content-Length", "17"},
        },
        .content_length = 17,
        .keep_alive = true,
    };

    assert(head.method == "POST");
    assert(head.target == "/api/users");
    assert(head.version == "HTTP/1.1");
    assert(head.headers.at("Content-Type") == "application/json");
    assert(head.headers.at("Content-Length") == "17");
    assert(head.content_length == 17);
    assert(head.keep_alive == true);
  }

  static void test_copy_preserves_all_fields()
  {
    ParsedRequestHead source;

    source.method = "PATCH";
    source.target = "/users/42";
    source.version = "HTTP/1.1";
    source.headers["Content-Type"] = "application/json";
    source.headers["Content-Length"] = "19";
    source.content_length = 19;
    source.keep_alive = false;

    ParsedRequestHead copy = source;

    assert(copy.method == "PATCH");
    assert(copy.target == "/users/42");
    assert(copy.version == "HTTP/1.1");
    assert(copy.headers.size() == 2);
    assert(copy.headers.at("Content-Type") == "application/json");
    assert(copy.headers.at("Content-Length") == "19");
    assert(copy.content_length == 19);
    assert(copy.keep_alive == false);

    copy.method = "PUT";
    copy.headers["Content-Length"] = "20";
    copy.content_length = 20;

    assert(source.method == "PATCH");
    assert(source.headers.at("Content-Length") == "19");
    assert(source.content_length == 19);

    assert(copy.method == "PUT");
    assert(copy.headers.at("Content-Length") == "20");
    assert(copy.content_length == 20);
  }

  static void test_move_preserves_all_fields()
  {
    ParsedRequestHead source;

    source.method = "DELETE";
    source.target = "/users/42";
    source.version = "HTTP/1.1";
    source.headers["Connection"] = "close";
    source.content_length = 0;
    source.keep_alive = false;

    ParsedRequestHead moved = std::move(source);

    assert(moved.method == "DELETE");
    assert(moved.target == "/users/42");
    assert(moved.version == "HTTP/1.1");
    assert(moved.headers.size() == 1);
    assert(moved.headers.at("Connection") == "close");
    assert(moved.content_length == 0);
    assert(moved.keep_alive == false);
  }

  static void test_copy_assignment_preserves_all_fields()
  {
    ParsedRequestHead source;

    source.method = "POST";
    source.target = "/submit";
    source.version = "HTTP/1.1";
    source.headers["Content-Length"] = "4";
    source.content_length = 4;
    source.keep_alive = true;

    ParsedRequestHead target;

    target.method = "GET";
    target.target = "/old";
    target.headers["Old"] = "yes";

    target = source;

    assert(target.method == "POST");
    assert(target.target == "/submit");
    assert(target.version == "HTTP/1.1");
    assert(target.headers.size() == 1);
    assert(target.headers.at("Content-Length") == "4");
    assert(target.content_length == 4);
    assert(target.keep_alive == true);
  }

  static void test_move_assignment_preserves_all_fields()
  {
    ParsedRequestHead source;

    source.method = "PUT";
    source.target = "/items/7";
    source.version = "HTTP/1.0";
    source.headers["Connection"] = "keep-alive";
    source.content_length = 12;
    source.keep_alive = true;

    ParsedRequestHead target;

    target.method = "GET";
    target.target = "/old";
    target.headers["Old"] = "yes";

    target = std::move(source);

    assert(target.method == "PUT");
    assert(target.target == "/items/7");
    assert(target.version == "HTTP/1.0");
    assert(target.headers.size() == 1);
    assert(target.headers.at("Connection") == "keep-alive");
    assert(target.content_length == 12);
    assert(target.keep_alive == true);
  }

  static void test_type_traits()
  {
    static_assert(std::is_default_constructible_v<ParsedRequestHead>);
    static_assert(std::is_copy_constructible_v<ParsedRequestHead>);
    static_assert(std::is_copy_assignable_v<ParsedRequestHead>);
    static_assert(std::is_move_constructible_v<ParsedRequestHead>);
    static_assert(std::is_move_assignable_v<ParsedRequestHead>);
    static_assert(std::is_destructible_v<ParsedRequestHead>);
  }

} // namespace

int main()
{
  test_default_parsed_request_head();
  test_set_request_line_fields();
  test_set_http_10_version();

  test_headers_can_be_added();
  test_headers_can_be_replaced();
  test_headers_are_case_sensitive_storage();

  test_content_length_defaults_and_can_be_set();
  test_content_length_large_value();

  test_keep_alive_can_be_disabled();
  test_keep_alive_can_be_enabled_again();

  test_aggregate_initialization_for_get_request();
  test_aggregate_initialization_for_post_request();

  test_copy_preserves_all_fields();
  test_move_preserves_all_fields();
  test_copy_assignment_preserves_all_fields();
  test_move_assignment_preserves_all_fields();

  test_type_traits();

  return 0;
}
