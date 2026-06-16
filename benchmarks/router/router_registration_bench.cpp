/**
 *
 * @file router_registration_bench.cpp
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
  using RouteRecord = vix::router::Router::RouteRecord;

  constexpr std::uint64_t kRouteRegistrations = 2'000;
  constexpr std::uint64_t kMixedRouteRegistrations = 2'500;
  constexpr std::uint64_t kDuplicateRegistrations = 2'000;
  constexpr std::uint64_t kRecordScanIterations = 50'000;

  constexpr vix::bench::BenchmarkConfig kRegistrationBenchConfig{
      .warmup_iterations = 1,
      .measure_iterations = 7,
  };

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

  static bool has_record(
      const std::vector<RouteRecord> &records,
      const std::string &method,
      const std::string &path)
  {
    for (const RouteRecord &record : records)
    {
      if (record.method == method &&
          record.path == path)
      {
        return true;
      }
    }

    return false;
  }

  static vix::bench::BenchmarkResult bench_register_static_routes()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/static_routes",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/items/" + std::to_string(i),
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/items/0"));
              assert(router.has_route("GET", "/items/" + std::to_string(kRouteRegistrations - 1u)));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/items/42"));

              vix::bench::do_not_optimize(sink);
            },
            kRegistrationBenchConfig);

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_nested_static_routes()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/nested_static_routes",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/api/v1/resources/" + std::to_string(i) + "/details",
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/v1/resources/0/details"));
              assert(router.has_route("GET", "/api/v1/resources/" + std::to_string(kRouteRegistrations - 1u) + "/details"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/v1/resources/42/details"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_param_routes()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/param_routes",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/api/v" + std::to_string(i) + "/users/{id}",
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/v0/users/42"));
              assert(router.has_route("GET", "/api/v" + std::to_string(kRouteRegistrations - 1u) + "/users/42"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/v42/users/gaspard"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_nested_param_routes()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/nested_param_routes",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/api/v" + std::to_string(i) + "/orgs/{org}/users/{user}/posts/{post}",
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/v0/orgs/vix/users/gaspard/posts/1"));
              assert(router.has_route("GET", "/api/v" + std::to_string(kRouteRegistrations - 1u) + "/orgs/vix/users/gaspard/posts/1"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/v42/orgs/vix/users/gaspard/posts/9"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_mixed_methods()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    const std::vector<std::string> methods{
        "GET",
        "POST",
        "PUT",
        "PATCH",
        "DELETE",
        "HEAD",
        "OPTIONS",
    };

    auto result =
        vix::bench::run(
            "router.registration/mixed_methods",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                const std::string &method =
                    methods[static_cast<std::size_t>(i % methods.size())];

                router.add_route(
                    method,
                    "/api/routes/" + std::to_string(i),
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/routes/0"));
              assert(router.has_route("POST", "/api/routes/1"));
              assert(router.has_route("PUT", "/api/routes/2"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("DELETE", "/api/routes/4"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_lowercase_methods()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    const std::vector<std::string> methods{
        "get",
        "post",
        "put",
        "patch",
        "delete",
    };

    auto result =
        vix::bench::run(
            "router.registration/lowercase_methods",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                const std::string &method =
                    methods[static_cast<std::size_t>(i % methods.size())];

                router.add_route(
                    method,
                    "/api/lowercase/" + std::to_string(i),
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/lowercase/0"));
              assert(router.has_route("POST", "/api/lowercase/1"));
              assert(router.has_route("PUT", "/api/lowercase/2"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("DELETE", "/api/lowercase/4"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_paths_without_leading_slash()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/path_without_leading_slash",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "api/no-slash/" + std::to_string(i),
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/no-slash/0"));
              assert(router.has_route("GET", "api/no-slash/42"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/no-slash/42"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_paths_with_trailing_slash()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/path_with_trailing_slash",
            kRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/api/trailing/" + std::to_string(i) + "/",
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations);
              assert(router.has_route("GET", "/api/trailing/0"));
              assert(router.has_route("GET", "/api/trailing/42/"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/trailing/42"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_duplicate_route()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/duplicate_route",
            kDuplicateRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kDuplicateRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/api/duplicate",
                    handler);
              }

              assert(router.routes().size() == kDuplicateRegistrations);
              assert(router.has_route("GET", "/api/duplicate"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/duplicate"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_static_and_param_same_prefix()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/static_and_param_same_prefix",
            kRouteRegistrations * 2u,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
              {
                router.add_route(
                    "GET",
                    "/api/users/" + std::to_string(i),
                    handler);

                router.add_route(
                    "GET",
                    "/api/users/" + std::to_string(i) + "/posts/{post}",
                    handler);
              }

              assert(router.routes().size() == kRouteRegistrations * 2u);
              assert(router.has_route("GET", "/api/users/42"));
              assert(router.has_route("GET", "/api/users/42/posts/abc"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/users/42"));
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/users/42/posts/abc"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_mixed_route_tree()
  {
    auto handler = make_handler();
    std::uint64_t sink = 0;

    const std::vector<std::string> methods{
        "GET",
        "POST",
        "PUT",
        "PATCH",
        "DELETE",
    };

    auto result =
        vix::bench::run(
            "router.registration/mixed_route_tree",
            kMixedRouteRegistrations,
            [&]()
            {
              Router router;

              for (std::uint64_t i = 0; i < kMixedRouteRegistrations; ++i)
              {
                const std::string &method =
                    methods[static_cast<std::size_t>(i % methods.size())];

                switch (i % 5u)
                {
                case 0u:
                  router.add_route(
                      method,
                      "/api/items/" + std::to_string(i),
                      handler);
                  break;

                case 1u:
                  router.add_route(
                      method,
                      "/api/items/" + std::to_string(i) + "/comments/{comment}",
                      handler);
                  break;

                case 2u:
                  router.add_route(
                      method,
                      "/api/orgs/{org}/projects/" + std::to_string(i),
                      handler);
                  break;

                case 3u:
                  router.add_route(
                      method,
                      "/assets/v" + std::to_string(i) + "/app.js",
                      handler);
                  break;

                default:
                  router.add_route(
                      method,
                      "/health/" + std::to_string(i),
                      handler);
                  break;
                }
              }

              assert(router.routes().size() == kMixedRouteRegistrations);
              assert(router.has_route("GET", "/api/items/0"));
              assert(router.has_route("POST", "/api/items/1/comments/abc"));
              assert(router.has_route("PUT", "/api/orgs/vix/projects/2"));

              sink += router.routes().size();
              sink += static_cast<std::uint64_t>(router.has_route("GET", "/api/items/0"));
              sink += static_cast<std::uint64_t>(router.has_route("POST", "/api/items/1/comments/abc"));
              sink += static_cast<std::uint64_t>(router.has_route("PUT", "/api/orgs/vix/projects/2"));

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_route_records_scan()
  {
    auto handler = make_handler();

    Router router;

    for (std::uint64_t i = 0; i < kRouteRegistrations; ++i)
    {
      router.add_route(
          "GET",
          "/api/records/" + std::to_string(i),
          handler);
    }

    assert(router.routes().size() == kRouteRegistrations);

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "router.registration/route_records_scan",
            kRecordScanIterations,
            [&]()
            {
              const auto &records = router.routes();

              for (std::uint64_t i = 0; i < kRecordScanIterations; ++i)
              {
                const std::string path =
                    "/api/records/" + std::to_string(i % kRouteRegistrations);

                const bool found =
                    has_record(
                        records,
                        "GET",
                        path);

                sink += static_cast<std::uint64_t>(found);
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

  results.push_back(bench_register_static_routes());
  results.push_back(bench_register_nested_static_routes());
  results.push_back(bench_register_param_routes());
  results.push_back(bench_register_nested_param_routes());
  results.push_back(bench_register_mixed_methods());
  results.push_back(bench_register_lowercase_methods());
  results.push_back(bench_register_paths_without_leading_slash());
  results.push_back(bench_register_paths_with_trailing_slash());
  results.push_back(bench_register_duplicate_route());
  results.push_back(bench_register_static_and_param_same_prefix());
  results.push_back(bench_register_mixed_route_tree());
  results.push_back(bench_route_records_scan());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.router.registration",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
