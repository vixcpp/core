/**
 *
 * @file http_response_bench.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include "common/Benchmark.hpp"
#include "common/BenchmarkJson.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/json.hpp>

namespace
{
  using Response = vix::http::Response;
  using HeaderMap = Response::HeaderMap;

  constexpr std::uint64_t kConstructIterations = 500'000;
  constexpr std::uint64_t kAccessorIterations = 2'000'000;
  constexpr std::uint64_t kHelperIterations = 300'000;
  constexpr std::uint64_t kSerializeIterations = 100'000;
  constexpr std::uint64_t kLargeSerializeIterations = 20'000;
  constexpr std::uint64_t kStatusIterations = 2'000'000;

  static HeaderMap make_headers()
  {
    return HeaderMap{
        {"Content-Type", "application/json; charset=utf-8"},
        {"X-Request-Id", "req-001"},
        {"X-Trace-Id", "trace-001"},
        {"Cache-Control", "no-store"},
        {"Connection", "keep-alive"},
    };
  }

  static HeaderMap make_many_headers()
  {
    HeaderMap headers;

    for (int i = 0; i < 32; ++i)
    {
      headers.emplace(
          "X-Bench-Header-" + std::to_string(i),
          "value-" + std::to_string(i));
    }

    headers.emplace("Content-Type", "application/json; charset=utf-8");
    headers.emplace("Connection", "keep-alive");

    return headers;
  }

  static std::string make_small_body()
  {
    return "hello from vix";
  }

  static std::string make_json_body()
  {
    return R"({"name":"Vix.cpp","version":"2.6.3","active":true,"count":42})";
  }

  static std::string make_large_body()
  {
    std::string body;
    body.reserve(64 * 1024);

    for (int i = 0; i < 1024; ++i)
    {
      body += "Vix.cpp benchmark response body line ";
      body += std::to_string(i);
      body += '\n';
    }

    return body;
  }

  static vix::json::Json make_json_payload()
  {
    return vix::json::Json{
        {"name", "Vix.cpp"},
        {"version", "2.6.3"},
        {"active", true},
        {"count", 42},
        {"tags", {"core", "http", "response"}},
    };
  }

  static vix::bench::BenchmarkResult bench_default_construct()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/default_construct",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response res;

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.version().size();
                sink += res.reason().size();
                sink += res.body().size();
                sink += res.headers().size();
                sink += static_cast<std::uint64_t>(res.should_close());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_status()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/construct_status",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response res{vix::http::CREATED};

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.version().size();
                sink += res.body().size();
                sink += res.headers().size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_status_body()
  {
    std::uint64_t sink = 0;

    const std::string body = make_small_body();

    auto result =
        vix::bench::run(
            "http.response/construct_status_body",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response res{
                    vix::http::OK,
                    body};

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.headers().size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_set_status_body_headers()
  {
    std::uint64_t sink = 0;

    const std::string body = make_json_body();

    auto result =
        vix::bench::run(
            "http.response/set_status_body_headers",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response res;

                res.set_status(vix::http::CREATED);
                res.set_body(body);
                res.set_header("Content-Type", "application/json; charset=utf-8");
                res.set_header("X-Request-Id", "req-001");
                res.set_header("Connection", "keep-alive");

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.headers().size();
                sink += res.header("Content-Type").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_replace_headers()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/replace_headers",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response res;

                res.set_headers(make_headers());

                sink += res.headers().size();
                sink += res.header("Content-Type").size();
                sink += res.header("X-Request-Id").size();

                res.clear_headers();

                sink += static_cast<std::uint64_t>(res.headers().empty());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_header_lookup_present()
  {
    Response res;

    res.set_headers(make_headers());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/header_lookup_present",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(res.has_header("Content-Type"));
                sink += res.header("Content-Type").size();

                sink += static_cast<std::uint64_t>(res.has_header("X-Request-Id"));
                sink += res.header("X-Request-Id").size();

                sink += static_cast<std::uint64_t>(res.has_header("X-Trace-Id"));
                sink += res.header("X-Trace-Id").size();

                sink += static_cast<std::uint64_t>(res.has_header("Connection"));
                sink += res.header("Connection").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_header_lookup_missing()
  {
    Response res;

    res.set_headers(make_headers());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/header_lookup_missing",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(!res.has_header("Missing"));
                sink += res.header("Missing").size();

                sink += static_cast<std::uint64_t>(!res.has_header("Authorization"));
                sink += res.header("Authorization").size();

                sink += static_cast<std::uint64_t>(!res.has_header("Cookie"));
                sink += res.header("Cookie").size();

                sink += static_cast<std::uint64_t>(!res.has_header("X-Missing"));
                sink += res.header("X-Missing").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_remove_header()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/remove_header",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response res;

                res.set_headers(make_headers());

                res.remove_header("X-Request-Id");
                res.remove_header("X-Trace-Id");
                res.remove_header("Missing");

                sink += static_cast<std::uint64_t>(!res.has_header("X-Request-Id"));
                sink += static_cast<std::uint64_t>(!res.has_header("X-Trace-Id"));
                sink += res.headers().size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_status_helpers()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/status_helpers",
            kStatusIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kStatusIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(vix::http::is_valid_status(200));
                sink += static_cast<std::uint64_t>(vix::http::normalize_status(200));
                sink += vix::http::reason_phrase(200).size();
                sink += vix::http::status_to_string(200).size();

                sink += static_cast<std::uint64_t>(vix::http::is_valid_status(404));
                sink += static_cast<std::uint64_t>(vix::http::normalize_status(404));
                sink += vix::http::reason_phrase(404).size();
                sink += vix::http::status_to_string(404).size();

                sink += static_cast<std::uint64_t>(vix::http::is_valid_status(500));
                sink += static_cast<std::uint64_t>(vix::http::normalize_status(500));
                sink += vix::http::reason_phrase(500).size();
                sink += vix::http::status_to_string(500).size();

                sink += static_cast<std::uint64_t>(!vix::http::is_valid_status(99));
                sink += static_cast<std::uint64_t>(!vix::http::is_valid_status(600));
                sink += vix::http::reason_phrase(777).size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_common_headers()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/common_headers",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                Response res;

                Response::common_headers(res);

                sink += static_cast<std::uint64_t>(res.has_header("Server"));
                sink += static_cast<std::uint64_t>(res.has_header("Date"));
                sink += res.header("Server").size();
                sink += res.header("Date").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_text_response_helper()
  {
    std::uint64_t sink = 0;

    const std::string body = make_small_body();

    auto result =
        vix::bench::run(
            "http.response/text_response_helper",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                Response res;

                Response::text_response(
                    res,
                    body,
                    vix::http::OK);

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.header("Content-Type").size();
                sink += res.header("Content-Length").size();
                sink += res.header("Server").size();
                sink += res.header("Date").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_json_response_helper()
  {
    std::uint64_t sink = 0;

    const vix::json::Json payload = make_json_payload();

    auto result =
        vix::bench::run(
            "http.response/json_response_helper",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                Response res;

                Response::json_response(
                    res,
                    payload,
                    vix::http::CREATED);

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.header("Content-Type").size();
                sink += res.header("Content-Length").size();
                sink += res.header("Server").size();
                sink += res.header("Date").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_error_response_helper()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/error_response_helper",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                Response res;

                Response::error_response(
                    res,
                    vix::http::NOT_FOUND,
                    "Route not found");

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.header("Content-Type").size();
                sink += res.header("Content-Length").size();
                sink += res.header("Server").size();
                sink += res.header("Date").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_redirect_response_helper()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/redirect_response_helper",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                Response res;

                Response::redirect_response(
                    res,
                    "/login");

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.header("Location").size();
                sink += res.header("Content-Type").size();
                sink += res.header("Content-Length").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_no_content_response_helper()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/no_content_response_helper",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                Response res;

                Response::no_content_response(res);

                sink += static_cast<std::uint64_t>(res.status());
                sink += res.body().size();
                sink += res.header("Content-Length").size();
                sink += res.header("Server").size();
                sink += res.header("Date").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_to_json_string()
  {
    std::uint64_t sink = 0;

    const vix::json::Json payload = make_json_payload();

    auto result =
        vix::bench::run(
            "http.response/to_json_string",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                const std::string json =
                    Response::to_json_string(payload);

                sink += json.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_http_date_now()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/http_date_now",
            kHelperIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kHelperIterations; ++i)
              {
                const std::string date =
                    Response::http_date_now();

                sink += date.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_to_http_string_text()
  {
    std::uint64_t sink = 0;

    Response base;

    base.set_status(vix::http::OK);
    base.set_body(make_small_body());
    base.set_header("Content-Type", "text/plain; charset=utf-8");

    auto result =
        vix::bench::run(
            "http.response/to_http_string_text",
            kSerializeIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSerializeIterations; ++i)
              {
                const std::string raw =
                    base.to_http_string();

                sink += raw.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_to_http_string_json()
  {
    std::uint64_t sink = 0;

    Response base;

    Response::json_response(
        base,
        make_json_payload(),
        vix::http::OK);

    auto result =
        vix::bench::run(
            "http.response/to_http_string_json",
            kSerializeIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSerializeIterations; ++i)
              {
                const std::string raw =
                    base.to_http_string();

                sink += raw.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_to_http_string_many_headers()
  {
    std::uint64_t sink = 0;

    Response base;

    base.set_status(vix::http::OK);
    base.set_body(make_json_body());
    base.set_headers(make_many_headers());

    auto result =
        vix::bench::run(
            "http.response/to_http_string_many_headers",
            kSerializeIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSerializeIterations; ++i)
              {
                const std::string raw =
                    base.to_http_string();

                sink += raw.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_to_http_string_large_body()
  {
    std::uint64_t sink = 0;

    Response base;

    base.set_status(vix::http::OK);
    base.set_body(make_large_body());
    base.set_header("Content-Type", "text/plain; charset=utf-8");

    auto result =
        vix::bench::run(
            "http.response/to_http_string_large_body",
            kLargeSerializeIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kLargeSerializeIterations; ++i)
              {
                const std::string raw =
                    base.to_http_string();

                sink += raw.size();
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_copy_response()
  {
    Response source;

    source.set_status(vix::http::CREATED);
    source.set_body(make_json_body());
    source.set_headers(make_headers());
    source.set_reason("Created");
    source.set_should_close(false);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.response/copy_response",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response copy = source;

                sink += static_cast<std::uint64_t>(copy.status());
                sink += copy.version().size();
                sink += copy.reason().size();
                sink += copy.body().size();
                sink += copy.headers().size();
                sink += static_cast<std::uint64_t>(copy.should_close());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_move_response()
  {
    std::uint64_t sink = 0;

    const std::string body = make_json_body();

    auto result =
        vix::bench::run(
            "http.response/move_response",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Response source;

                source.set_status(vix::http::CREATED);
                source.set_body(body);
                source.set_headers(make_headers());
                source.set_reason("Created");
                source.set_should_close(false);

                Response moved = std::move(source);

                sink += static_cast<std::uint64_t>(moved.status());
                sink += moved.version().size();
                sink += moved.reason().size();
                sink += moved.body().size();
                sink += moved.headers().size();
                sink += static_cast<std::uint64_t>(moved.should_close());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_default_construct());
  results.push_back(bench_construct_status());
  results.push_back(bench_construct_status_body());
  results.push_back(bench_set_status_body_headers());
  results.push_back(bench_replace_headers());

  results.push_back(bench_header_lookup_present());
  results.push_back(bench_header_lookup_missing());
  results.push_back(bench_remove_header());

  results.push_back(bench_status_helpers());
  results.push_back(bench_common_headers());
  results.push_back(bench_text_response_helper());
  results.push_back(bench_json_response_helper());
  results.push_back(bench_error_response_helper());
  results.push_back(bench_redirect_response_helper());
  results.push_back(bench_no_content_response_helper());
  results.push_back(bench_to_json_string());
  results.push_back(bench_http_date_now());

  results.push_back(bench_to_http_string_text());
  results.push_back(bench_to_http_string_json());
  results.push_back(bench_to_http_string_many_headers());
  results.push_back(bench_to_http_string_large_body());

  results.push_back(bench_copy_response());
  results.push_back(bench_move_response());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.http.response",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
