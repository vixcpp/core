/**
 *
 * @file http_request_bench.cpp
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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vix/http/Request.hpp>
#include <vix/http/RequestState.hpp>
#include <vix/json/json.hpp>

namespace
{
  using Request = vix::http::Request;
  using HeaderMap = Request::HeaderMap;
  using ParamMap = Request::ParamMap;
  using StatePtr = Request::StatePtr;

  constexpr std::uint64_t kConstructIterations = 500'000;
  constexpr std::uint64_t kAccessorIterations = 2'000'000;
  constexpr std::uint64_t kJsonIterations = 100'000;
  constexpr std::uint64_t kStateIterations = 1'000'000;

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

  static HeaderMap make_headers()
  {
    return HeaderMap{
        {"Host", "localhost"},
        {"User-Agent", "vix-bench"},
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"Content-Length", "83"},
        {"X-Request-Id", "req-001"},
        {"X-Trace-Id", "trace-001"},
        {"Connection", "keep-alive"},
    };
  }

  static ParamMap make_params()
  {
    return ParamMap{
        {"id", "42"},
        {"org", "vix"},
        {"user", "gaspard"},
        {"post", "100"},
    };
  }

  static std::string make_json_body()
  {
    return R"({"name":"Vix.cpp","version":"2.6.3","active":true,"count":42,"tags":["core","http"]})";
  }

  static vix::bench::BenchmarkResult bench_default_construct()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/default_construct",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request req;

                sink += req.method().size();
                sink += req.target().size();
                sink += req.path().size();
                sink += req.query_string().size();
                sink += req.body().size();
                sink += req.headers().size();
                sink += req.params().size();
                sink += req.query().size();
                sink += static_cast<std::uint64_t>(req.has_state());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kConstructIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_static_target()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/construct_static_target",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request req =
                    make_request(
                        "GET",
                        "/api/status",
                        make_headers());

                sink += req.method().size();
                sink += req.target().size();
                sink += req.path().size();
                sink += req.headers().size();
                sink += static_cast<std::uint64_t>(req.has_state());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_query_target()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/construct_query_target",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request req =
                    make_request(
                        "GET",
                        "/api/search?q=vix&page=2&limit=25&active=true",
                        make_headers());

                sink += req.method().size();
                sink += req.target().size();
                sink += req.path().size();
                sink += req.query_string().size();
                sink += req.query().size();
                sink += static_cast<std::uint64_t>(req.has_query("q"));
                sink += static_cast<std::uint64_t>(req.has_query("page"));
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_with_params()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/construct_with_params",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request req =
                    make_request(
                        "GET",
                        "/api/orgs/vix/users/gaspard/posts/100",
                        make_headers(),
                        std::string{},
                        make_params());

                sink += req.method().size();
                sink += req.target().size();
                sink += req.path().size();
                sink += req.params().size();
                sink += static_cast<std::uint64_t>(req.has_param("org"));
                sink += static_cast<std::uint64_t>(req.has_param("user"));
                sink += static_cast<std::uint64_t>(req.has_param("post"));
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_construct_json_body()
  {
    std::uint64_t sink = 0;

    const std::string body = make_json_body();

    auto result =
        vix::bench::run(
            "http.request/construct_json_body",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request req =
                    make_request(
                        "POST",
                        "/api/items",
                        make_headers(),
                        body);

                sink += req.method().size();
                sink += req.target().size();
                sink += req.path().size();
                sink += req.body().size();
                sink += req.headers().size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_basic_accessors()
  {
    Request req =
        make_request(
            "GET",
            "/api/search?q=vix&page=2&limit=25&active=true",
            make_headers(),
            std::string{},
            make_params());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/basic_accessors",
            kAccessorIterations * 5u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += req.method().size();
                sink += req.target().size();
                sink += req.path().size();
                sink += req.query_string().size();
                sink += req.body().size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_header_lookup_present()
  {
    Request req =
        make_request(
            "POST",
            "/api/items",
            make_headers(),
            make_json_body());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/header_lookup_present",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(req.has_header("Host"));
                sink += req.header("Host").size();

                sink += static_cast<std::uint64_t>(req.has_header("Content-Type"));
                sink += req.header("Content-Type").size();

                sink += static_cast<std::uint64_t>(req.has_header("X-Request-Id"));
                sink += req.header("X-Request-Id").size();

                sink += static_cast<std::uint64_t>(req.has_header("Connection"));
                sink += req.header("Connection").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_header_lookup_missing()
  {
    Request req =
        make_request(
            "GET",
            "/api/status",
            make_headers());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/header_lookup_missing",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(!req.has_header("Missing"));
                sink += req.header("Missing").size();

                sink += static_cast<std::uint64_t>(!req.has_header("Authorization"));
                sink += req.header("Authorization").size();

                sink += static_cast<std::uint64_t>(!req.has_header("X-Missing"));
                sink += req.header("X-Missing").size();

                sink += static_cast<std::uint64_t>(!req.has_header("Cookie"));
                sink += req.header("Cookie").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_param_lookup_present()
  {
    Request req =
        make_request(
            "GET",
            "/api/orgs/vix/users/gaspard/posts/100",
            make_headers(),
            std::string{},
            make_params());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/param_lookup_present",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(req.has_param("id"));
                sink += req.param("id").size();

                sink += static_cast<std::uint64_t>(req.has_param("org"));
                sink += req.param("org").size();

                sink += static_cast<std::uint64_t>(req.has_param("user"));
                sink += req.param("user").size();

                sink += static_cast<std::uint64_t>(req.has_param("post"));
                sink += req.param("post").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_param_lookup_missing()
  {
    Request req =
        make_request(
            "GET",
            "/api/orgs/vix/users/gaspard/posts/100",
            make_headers(),
            std::string{},
            make_params());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/param_lookup_missing",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(!req.has_param("missing"));
                sink += req.param("missing").size();

                sink += static_cast<std::uint64_t>(!req.has_param("slug"));
                sink += req.param("slug").size();

                sink += static_cast<std::uint64_t>(!req.has_param("project"));
                sink += req.param("project", "fallback").size();

                sink += static_cast<std::uint64_t>(!req.has_param("team"));
                sink += req.param("team", "fallback").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_query_lookup_present()
  {
    Request req =
        make_request(
            "GET",
            "/api/search?q=vix&page=2&limit=25&active=true",
            make_headers());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/query_lookup_present",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(req.has_query("q"));
                sink += req.query_value("q").size();

                sink += static_cast<std::uint64_t>(req.has_query("page"));
                sink += req.query_value("page").size();

                sink += static_cast<std::uint64_t>(req.has_query("limit"));
                sink += req.query_value("limit").size();

                sink += static_cast<std::uint64_t>(req.has_query("active"));
                sink += req.query_value("active").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_query_lookup_missing()
  {
    Request req =
        make_request(
            "GET",
            "/api/search?q=vix&page=2&limit=25&active=true",
            make_headers());

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/query_lookup_missing",
            kAccessorIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(!req.has_query("missing"));
                sink += req.query_value("missing").size();

                sink += static_cast<std::uint64_t>(!req.has_query("sort"));
                sink += req.query_value("sort").size();

                sink += static_cast<std::uint64_t>(!req.has_query("filter"));
                sink += req.query_value("filter", "all").size();

                sink += static_cast<std::uint64_t>(!req.has_query("offset"));
                sink += req.query_value("offset", "0").size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_json_parse_new_request()
  {
    const std::string body = make_json_body();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/json_parse_new_request",
            kJsonIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kJsonIterations; ++i)
              {
                Request req =
                    make_request(
                        "POST",
                        "/api/items",
                        make_headers(),
                        body);

                const auto &json = req.json();

                sink += json["name"].get<std::string>().size();
                sink += static_cast<std::uint64_t>(json["active"].get<bool>());
                sink += static_cast<std::uint64_t>(json["count"].get<int>());
                sink += json["tags"].size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_json_cached_access()
  {
    Request req =
        make_request(
            "POST",
            "/api/items",
            make_headers(),
            make_json_body());

    const auto &first = req.json();

    assert(first["name"].get<std::string>() == "Vix.cpp");

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/json_cached_access",
            kAccessorIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kAccessorIterations; ++i)
              {
                const auto &json = req.json();

                sink += json["name"].get<std::string>().size();
                sink += static_cast<std::uint64_t>(json["active"].get<bool>());
                sink += static_cast<std::uint64_t>(json["count"].get<int>());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_state_has_get()
  {
    Request req =
        make_request(
            "GET",
            "/api/status",
            make_headers());

    req.set_state<UserId>(UserId{42});
    req.set_state<TraceId>(TraceId{"trace-001"});

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/state_has_get",
            kStateIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kStateIterations; ++i)
              {
                sink += static_cast<std::uint64_t>(req.has_state_type<UserId>());
                sink += static_cast<std::uint64_t>(req.has_state_type<TraceId>());

                sink += static_cast<std::uint64_t>(req.state<UserId>().value);
                sink += req.state<TraceId>().value.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_state_try_get()
  {
    Request req =
        make_request(
            "GET",
            "/api/status",
            make_headers());

    req.set_state<UserId>(UserId{42});
    req.set_state<TraceId>(TraceId{"trace-001"});

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/state_try_get",
            kStateIterations * 4u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kStateIterations; ++i)
              {
                if (const UserId *user = req.try_state<UserId>())
                {
                  sink += static_cast<std::uint64_t>(user->value);
                }

                if (const TraceId *trace = req.try_state<TraceId>())
                {
                  sink += trace->value.size();
                }

                sink += static_cast<std::uint64_t>(req.try_state<int>() == nullptr);
                sink += static_cast<std::uint64_t>(req.try_state<std::string>() == nullptr);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_state_emplace()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/state_emplace",
            kStateIterations,
            [&]()
            {
              Request req =
                  make_request(
                      "GET",
                      "/api/status",
                      make_headers());

              for (std::uint64_t i = 0; i < kStateIterations; ++i)
              {
                UserId &user =
                    req.emplace_state<UserId>(
                        static_cast<int>(i));

                TraceId &trace =
                    req.emplace_state<TraceId>(
                        "trace-" + std::to_string(i));

                sink += static_cast<std::uint64_t>(user.value);
                sink += trace.value.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_copy_request()
  {
    Request source =
        make_request(
            "POST",
            "/api/items?trace=1",
            make_headers(),
            make_json_body(),
            make_params());

    source.set_state<UserId>(UserId{42});
    source.set_state<TraceId>(TraceId{"trace-copy"});

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/copy_request",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request copy = source;

                sink += copy.method().size();
                sink += copy.target().size();
                sink += copy.path().size();
                sink += copy.query_string().size();
                sink += copy.headers().size();
                sink += copy.params().size();
                sink += copy.body().size();
                sink += static_cast<std::uint64_t>(copy.has_state());
                sink += static_cast<std::uint64_t>(copy.has_state_type<UserId>());
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_move_request()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "http.request/move_request",
            kConstructIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kConstructIterations; ++i)
              {
                Request source =
                    make_request(
                        "POST",
                        "/api/items?trace=1",
                        make_headers(),
                        make_json_body(),
                        make_params());

                source.set_state<UserId>(UserId{42});
                source.set_state<TraceId>(TraceId{"trace-move"});

                Request moved = std::move(source);

                sink += moved.method().size();
                sink += moved.target().size();
                sink += moved.path().size();
                sink += moved.query_string().size();
                sink += moved.headers().size();
                sink += moved.params().size();
                sink += moved.body().size();
                sink += static_cast<std::uint64_t>(moved.has_state());
                sink += static_cast<std::uint64_t>(moved.has_state_type<TraceId>());
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
  results.push_back(bench_construct_static_target());
  results.push_back(bench_construct_query_target());
  results.push_back(bench_construct_with_params());
  results.push_back(bench_construct_json_body());

  results.push_back(bench_basic_accessors());
  results.push_back(bench_header_lookup_present());
  results.push_back(bench_header_lookup_missing());
  results.push_back(bench_param_lookup_present());
  results.push_back(bench_param_lookup_missing());
  results.push_back(bench_query_lookup_present());
  results.push_back(bench_query_lookup_missing());

  results.push_back(bench_json_parse_new_request());
  results.push_back(bench_json_cached_access());

  results.push_back(bench_state_has_get());
  results.push_back(bench_state_try_get());
  results.push_back(bench_state_emplace());

  results.push_back(bench_copy_request());
  results.push_back(bench_move_request());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.http.request",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
