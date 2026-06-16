/**
 *
 * @file router_match_bench.cpp
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
#include <vector>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/Router.hpp>

namespace
{
  using Request = vix::http::Request;
  using Response = vix::http::Response;
  using Router = vix::router::Router;

  constexpr std::uint64_t kMatchIterations = 1'000'000;
  constexpr std::uint64_t kManyRoutes = 1'000;
  constexpr std::uint64_t kManyRouteIterations = 500'000;

  class DummyHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &,
        Response &) override
    {
      co_return;
    }
  };

  static std::shared_ptr<vix::http::IRequestHandler> make_handler()
  {
    return std::make_shared<DummyHandler>();
  }

  static void add_standard_routes(Router &router)
  {
    router.add_route("GET", "/", make_handler());
    router.add_route("GET", "/health", make_handler());
    router.add_route("GET", "/api/status", make_handler());
    router.add_route("GET", "/api/users", make_handler());
    router.add_route("POST", "/api/users", make_handler());
    router.add_route("GET", "/api/users/{id}", make_handler());
    router.add_route("GET", "/api/users/me", make_handler());
    router.add_route("PUT", "/api/users/{id}", make_handler());
    router.add_route("DELETE", "/api/users/{id}", make_handler());
    router.add_route("GET", "/api/orgs/{org}/users/{user}", make_handler());
    router.add_route("GET", "/api/search", make_handler());
    router.add_route("GET", "/assets/app.js", make_handler());
    router.add_route("GET", "/assets/app.css", make_handler());
  }

  static vix::bench::BenchmarkResult bench_strip_query()
  {
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/strip_query",
            kMatchIterations * 6u,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const std::string a = Router::strip_query("");
                const std::string b = Router::strip_query("/");
                const std::string c = Router::strip_query("users/42");
                const std::string d = Router::strip_query("/users/42/");
                const std::string e = Router::strip_query("/users/42?active=1&page=2");
                const std::string f = Router::strip_query("/search?q=a?b");

                sink += a.size();
                sink += b.size();
                sink += c.size();
                sink += d.size();
                sink += e.size();
                sink += f.size();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_static_route_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/static_route",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("GET", "/health");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_nested_static_route_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/nested_static_route",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("GET", "/api/status");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_param_route_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/param_route",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("GET", "/api/users/42");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_nested_param_route_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/nested_param_route",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("GET", "/api/orgs/vix/users/gaspard");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_static_wins_over_param_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/static_wins_over_param",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("GET", "/api/users/me");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_query_string_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/query_string",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("GET", "/api/search?q=vix&page=2");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_method_case_normalization_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/method_case_normalization",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool matched =
                    router.has_route("gEt", "/api/status");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_missing_route_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/missing_route",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool missing =
                    !router.has_route("GET", "/api/missing/route");

                sink += static_cast<std::uint64_t>(missing);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_wrong_method_match()
  {
    Router router;
    add_standard_routes(router);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/wrong_method",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const bool missing =
                    !router.has_route("PATCH", "/api/status");

                sink += static_cast<std::uint64_t>(missing);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kMatchIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_many_static_routes_match()
  {
    Router router;

    for (std::uint64_t i = 0; i < kManyRoutes; ++i)
    {
      router.add_route(
          "GET",
          "/items/" + std::to_string(i),
          make_handler());
    }

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/many_static_routes",
            kManyRouteIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kManyRouteIterations; ++i)
              {
                const std::uint64_t id = i % kManyRoutes;

                const bool matched =
                    router.has_route(
                        "GET",
                        "/items/" + std::to_string(id));

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kManyRouteIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_many_param_routes_match()
  {
    Router router;

    for (std::uint64_t i = 0; i < kManyRoutes; ++i)
    {
      router.add_route(
          "GET",
          "/api/v" + std::to_string(i) + "/users/{id}",
          make_handler());
    }

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/many_param_routes",
            kManyRouteIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kManyRouteIterations; ++i)
              {
                const std::uint64_t id = i % kManyRoutes;

                const bool matched =
                    router.has_route(
                        "GET",
                        "/api/v" + std::to_string(id) + "/users/42");

                sink += static_cast<std::uint64_t>(matched);
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink >= kManyRouteIterations);

    return result;
  }

  static vix::bench::BenchmarkResult bench_mixed_route_match()
  {
    Router router;
    add_standard_routes(router);

    for (std::uint64_t i = 0; i < 100u; ++i)
    {
      router.add_route(
          "GET",
          "/resources/" + std::to_string(i),
          make_handler());

      router.add_route(
          "GET",
          "/resources/" + std::to_string(i) + "/items/{id}",
          make_handler());
    }

    const std::vector<std::string> paths{
        "/",
        "/health",
        "/api/status",
        "/api/users",
        "/api/users/42",
        "/api/users/me",
        "/api/orgs/vix/users/gaspard",
        "/api/search?q=vix",
        "/assets/app.js",
        "/resources/77",
        "/resources/77/items/abc",
        "/missing",
    };

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.match/mixed_routes",
            kMatchIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kMatchIterations; ++i)
              {
                const std::string &path =
                    paths[static_cast<std::size_t>(i % paths.size())];

                const bool matched =
                    router.has_route("GET", path);

                sink += static_cast<std::uint64_t>(matched);
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

  results.push_back(bench_strip_query());
  results.push_back(bench_static_route_match());
  results.push_back(bench_nested_static_route_match());
  results.push_back(bench_param_route_match());
  results.push_back(bench_nested_param_route_match());
  results.push_back(bench_static_wins_over_param_match());
  results.push_back(bench_query_string_match());
  results.push_back(bench_method_case_normalization_match());
  results.push_back(bench_missing_route_match());
  results.push_back(bench_wrong_method_match());
  results.push_back(bench_many_static_routes_match());
  results.push_back(bench_many_param_routes_match());
  results.push_back(bench_mixed_route_match());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.router.match",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
