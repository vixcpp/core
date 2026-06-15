/**
 *
 * @file request_handler_helpers_test.cpp
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
#include <tuple>
#include <utility>

#include <vix/http/RequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/json.hpp>

namespace
{
  using Request = vix::http::Request;
  using ResponseWrapper = vix::http::ResponseWrapper;

  struct VoidReqResHandler
  {
    void operator()(Request &, ResponseWrapper &) const {}
  };

  struct VoidReqResParamsHandler
  {
    void operator()(Request &, ResponseWrapper &, const Request::ParamMap &) const {}
  };

  struct ReturnTextHandler
  {
    std::string operator()(Request &, ResponseWrapper &) const
    {
      return "ok";
    }
  };

  struct ReturnStatusTextPairHandler
  {
    std::pair<int, std::string> operator()(Request &, ResponseWrapper &) const
    {
      return {vix::http::CREATED, "created"};
    }
  };

  struct ReturnStatusJsonTupleHandler
  {
    std::tuple<int, vix::json::Json> operator()(
        Request &,
        ResponseWrapper &,
        const Request::ParamMap &) const
    {
      return {
          vix::http::ACCEPTED,
          vix::json::Json{
              {"ok", true},
          }};
    }
  };

  static bool contains(const std::string &text, const std::string &needle)
  {
    return text.find(needle) != std::string::npos;
  }

  static void test_handler_concepts_for_supported_signatures()
  {
    static_assert(vix::http::HttpStatusLike<int>);
    static_assert(vix::http::HttpStatusLike<unsigned short>);
    static_assert(vix::http::ReturnSendable<std::string>);
    static_assert(vix::http::ReturnSendable<const char *>);
    static_assert(vix::http::ReturnSendable<vix::json::Json>);

    static_assert(vix::http::StatusPayloadPair<std::pair<int, std::string>>);
    static_assert(vix::http::StatusPayloadPair<std::pair<unsigned short, const char *>>);

    static_assert(vix::http::StatusPayloadTuple<std::tuple<int, std::string>>);
    static_assert(vix::http::StatusPayloadTuple<std::tuple<int, vix::json::Json>>);

    static_assert(vix::http::Returnable<std::string>);
    static_assert(vix::http::Returnable<const char *>);
    static_assert(vix::http::Returnable<vix::json::Json>);
    static_assert(vix::http::Returnable<std::pair<int, std::string>>);
    static_assert(vix::http::Returnable<std::tuple<int, vix::json::Json>>);

    static_assert(vix::http::HandlerReqResVoid<VoidReqResHandler>);
    static_assert(vix::http::HandlerReqResParamsVoid<VoidReqResParamsHandler>);
    static_assert(vix::http::HandlerReqResRet<ReturnTextHandler>);
    static_assert(vix::http::HandlerReqResRet<ReturnStatusTextPairHandler>);
    static_assert(vix::http::HandlerReqResParamsRet<ReturnStatusJsonTupleHandler>);

    static_assert(vix::http::ValidHandler<VoidReqResHandler>);
    static_assert(vix::http::ValidHandler<VoidReqResParamsHandler>);
    static_assert(vix::http::ValidHandler<ReturnTextHandler>);
    static_assert(vix::http::ValidHandler<ReturnStatusTextPairHandler>);
    static_assert(vix::http::ValidHandler<ReturnStatusJsonTupleHandler>);
  }

  static void test_extract_params_from_simple_route()
  {
    const auto params = vix::http::extract_params_from_path(
        "/users/{id}",
        "/users/42");

    assert(params.size() == 1);
    assert(params.at("id") == "42");
  }

  static void test_extract_params_from_multiple_segments()
  {
    const auto params = vix::http::extract_params_from_path(
        "/posts/{post_id}/comments/{comment_id}",
        "/posts/10/comments/99");

    assert(params.size() == 2);
    assert(params.at("post_id") == "10");
    assert(params.at("comment_id") == "99");
  }

  static void test_extract_params_from_root_param()
  {
    const auto params = vix::http::extract_params_from_path(
        "/{name}",
        "/vix");

    assert(params.size() == 1);
    assert(params.at("name") == "vix");
  }

  static void test_extract_params_with_leading_trailing_and_repeated_slashes()
  {
    const auto params = vix::http::extract_params_from_path(
        "///users///{id}///profile///",
        "/users/42/profile/");

    assert(params.size() == 1);
    assert(params.at("id") == "42");
  }

  static void test_extract_params_preserves_encoded_values()
  {
    const auto params = vix::http::extract_params_from_path(
        "/files/{name}",
        "/files/hello%20world.txt");

    assert(params.size() == 1);
    assert(params.at("name") == "hello%20world.txt");
  }

  static void test_extract_params_preserves_dots_and_dashes()
  {
    const auto params = vix::http::extract_params_from_path(
        "/packages/{name}/versions/{version}",
        "/packages/vix-core/versions/2.6.3-beta.1");

    assert(params.size() == 2);
    assert(params.at("name") == "vix-core");
    assert(params.at("version") == "2.6.3-beta.1");
  }

  static void test_extract_params_allows_param_names_with_symbols()
  {
    const auto params = vix::http::extract_params_from_path(
        "/repos/{owner_name}/{repo-name}",
        "/repos/vixcpp/vix-core");

    assert(params.size() == 2);
    assert(params.at("owner_name") == "vixcpp");
    assert(params.at("repo-name") == "vix-core");
  }

  static void test_extract_params_repeated_param_name_keeps_first_value()
  {
    const auto params = vix::http::extract_params_from_path(
        "/{id}/{id}",
        "/first/second");

    assert(params.size() == 1);
    assert(params.at("id") == "first");
  }

  static void test_extract_params_static_route_match_returns_empty_map()
  {
    const auto params = vix::http::extract_params_from_path(
        "/health",
        "/health");

    assert(params.empty());
  }

  static void test_extract_params_root_route_match_returns_empty_map()
  {
    const auto params = vix::http::extract_params_from_path(
        "/",
        "/");

    assert(params.empty());
  }

  static void test_extract_params_mismatch_static_segment_returns_empty_map()
  {
    const auto params = vix::http::extract_params_from_path(
        "/users/{id}",
        "/projects/42");

    assert(params.empty());
  }

  static void test_extract_params_mismatch_segment_count_returns_empty_map()
  {
    {
      const auto params = vix::http::extract_params_from_path(
          "/users/{id}",
          "/users/42/profile");

      assert(params.empty());
    }

    {
      const auto params = vix::http::extract_params_from_path(
          "/users/{id}/profile",
          "/users/42");

      assert(params.empty());
    }
  }

  static void test_extract_params_empty_actual_segment_does_not_match_param()
  {
    const auto params = vix::http::extract_params_from_path(
        "/users/{id}",
        "/users//");

    assert(params.empty());
  }

  static void test_extract_params_empty_pattern_and_empty_path()
  {
    const auto params = vix::http::extract_params_from_path(
        "",
        "");

    assert(params.empty());
  }

  static void test_extract_params_empty_pattern_does_not_match_non_empty_path()
  {
    const auto params = vix::http::extract_params_from_path(
        "",
        "/users");

    assert(params.empty());
  }

  static void test_extract_params_literal_braces_without_valid_param()
  {
    {
      const auto params = vix::http::extract_params_from_path(
          "/users/{}",
          "/users/{}");

      assert(params.empty());
    }

    {
      const auto params = vix::http::extract_params_from_path(
          "/users/{}",
          "/users/42");

      assert(params.empty());
    }
  }

  static void test_extract_params_treats_query_part_as_literal_when_passed()
  {
    const auto params = vix::http::extract_params_from_path(
        "/users/{id}",
        "/users/42?active=1");

    assert(params.size() == 1);
    assert(params.at("id") == "42?active=1");
  }

  static void test_make_dev_error_html_contains_debug_context()
  {
    const std::string html = vix::http::make_dev_error_html(
        "Error",
        "something failed",
        "/users/{id}",
        "GET",
        "/users/42");

    assert(contains(html, "<!DOCTYPE html>"));
    assert(contains(html, "<html lang=\"en\">"));
    assert(contains(html, "<title>Error</title>"));
    assert(contains(html, "<pre>"));

    assert(contains(html, "Error: something failed"));
    assert(contains(html, "Route: /users/{id}"));
    assert(contains(html, "Method: GET"));
    assert(contains(html, "Path: /users/42"));

    assert(contains(html, "</pre></body></html>"));
  }

  static void test_make_dev_error_html_keeps_runtime_values()
  {
    const std::string html = vix::http::make_dev_error_html(
        "RangeError",
        "Invalid HTTP status code",
        "/orders/{order_id}/items/{item_id}",
        "POST",
        "/orders/10/items/5");

    assert(contains(html, "RangeError: Invalid HTTP status code"));
    assert(contains(html, "Route: /orders/{order_id}/items/{item_id}"));
    assert(contains(html, "Method: POST"));
    assert(contains(html, "Path: /orders/10/items/5"));
  }

} // namespace

int main()
{
  test_handler_concepts_for_supported_signatures();

  test_extract_params_from_simple_route();
  test_extract_params_from_multiple_segments();
  test_extract_params_from_root_param();
  test_extract_params_with_leading_trailing_and_repeated_slashes();
  test_extract_params_preserves_encoded_values();
  test_extract_params_preserves_dots_and_dashes();
  test_extract_params_allows_param_names_with_symbols();
  test_extract_params_repeated_param_name_keeps_first_value();
  test_extract_params_static_route_match_returns_empty_map();
  test_extract_params_root_route_match_returns_empty_map();
  test_extract_params_mismatch_static_segment_returns_empty_map();
  test_extract_params_mismatch_segment_count_returns_empty_map();
  test_extract_params_empty_actual_segment_does_not_match_param();
  test_extract_params_empty_pattern_and_empty_path();
  test_extract_params_empty_pattern_does_not_match_non_empty_path();
  test_extract_params_literal_braces_without_valid_param();
  test_extract_params_treats_query_part_as_literal_when_passed();

  test_make_dev_error_html_contains_debug_context();
  test_make_dev_error_html_keeps_runtime_values();

  return 0;
}
