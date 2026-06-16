/**
 *
 * @file app_route_registration_bench.cpp
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
#include <vector>

#include <vix/app/App.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/router/Router.hpp>

namespace
{
  using App = vix::App;
  using Request = vix::http::Request;
  using ResponseWrapper = vix::http::ResponseWrapper;
  using RouteRecord = vix::router::Router::RouteRecord;

  constexpr std::uint64_t kRoutesPerApp = 500;
  constexpr std::uint64_t kRegistrationIterations = 100;
  constexpr std::uint64_t kDuplicateRoutesPerApp = 500;
  constexpr std::uint64_t kRecordScanIterations = 500'000;

  static void set_env_var(
      const char *name,
      const std::string &value)
  {
#if defined(_WIN32)
    const std::string assignment =
        std::string{name} + "=" + value;

    const int rc =
        _putenv(assignment.c_str());

    assert(rc == 0);
#else
    const int rc =
        setenv(
            name,
            value.c_str(),
            1);

    assert(rc == 0);
#endif
  }

  static void unset_env_var(const char *name)
  {
#if defined(_WIN32)
    const std::string assignment =
        std::string{name} + "=";

    const int rc =
        _putenv(assignment.c_str());

    assert(rc == 0);
#else
    const int rc =
        unsetenv(name);

    assert(rc == 0);
#endif
  }

  static void prepare_app_env()
  {
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("VIX_DOCS", "false");
    set_env_var("VIX_ACCESS_LOGS", "false");
    set_env_var("VIX_INTERNAL_LOGS", "false");
    set_env_var("VIX_LOG_ASYNC", "false");
    set_env_var("VIX_LOG_LEVEL", "critical");

    unset_env_var("SERVER_PORT");
    unset_env_var("SERVER_TLS_ENABLED");
    unset_env_var("SERVER_TLS_CERT_FILE");
    unset_env_var("SERVER_TLS_KEY_FILE");
  }

  static void text_handler(
      Request &,
      ResponseWrapper &res)
  {
    res.ok().text("ok");
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

  static std::size_t count_records(
      const std::vector<RouteRecord> &records,
      const std::string &method,
      const std::string &path)
  {
    std::size_t count = 0;

    for (const RouteRecord &record : records)
    {
      if (record.method == method &&
          record.path == path)
      {
        ++count;
      }
    }

    return count;
  }

  static vix::bench::BenchmarkResult bench_app_construct_close()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "app.route_registration/construct_close",
            kRegistrationIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kRegistrationIterations; ++i)
              {
                App app;

                assert(app.router() != nullptr);
                assert(app.executor().started());
                assert(app.executor().running());
                assert(app.is_running() == false);

                sink += static_cast<std::uint64_t>(app.router() != nullptr);
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/bench"));
                sink += app.router()->routes().size();

                app.close();

                assert(app.is_running() == false);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 1,
                .measure_iterations = 7,
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_get_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/get_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  app.get(
                      "/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/items/0"));
                assert(app.router()->has_route("GET", "/items/" + std::to_string(kRoutesPerApp - 1u)));
                assert(app.router()->has_route("OPTIONS", "/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/items/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/items/42"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_post_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/post_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  app.post(
                      "/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("POST", "/items/0"));
                assert(app.router()->has_route("POST", "/items/" + std::to_string(kRoutesPerApp - 1u)));
                assert(app.router()->has_route("OPTIONS", "/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("POST", "/items/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/items/42"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_put_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/put_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  app.put(
                      "/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("PUT", "/items/0"));
                assert(app.router()->has_route("PUT", "/items/" + std::to_string(kRoutesPerApp - 1u)));
                assert(app.router()->has_route("OPTIONS", "/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("PUT", "/items/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/items/42"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_param_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/param_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  app.get(
                      "/api/v" + std::to_string(i) + "/users/{id}",
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v0/users/42"));
                assert(app.router()->has_route("GET", "/api/v" + std::to_string(kRoutesPerApp - 1u) + "/users/42"));
                assert(app.router()->has_route("OPTIONS", "/api/v0/users/42"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/v42/users/gaspard"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/v42/users/gaspard"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_nested_param_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/nested_param_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  app.get(
                      "/api/v" + std::to_string(i) + "/orgs/{org}/users/{user}/posts/{post}",
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v0/orgs/vix/users/gaspard/posts/1"));
                assert(app.router()->has_route("GET", "/api/v" + std::to_string(kRoutesPerApp - 1u) + "/orgs/vix/users/gaspard/posts/1"));
                assert(app.router()->has_route("OPTIONS", "/api/v0/orgs/vix/users/gaspard/posts/1"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/v42/orgs/vix/users/gaspard/posts/9"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/v42/orgs/vix/users/gaspard/posts/9"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_mixed_methods()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/mixed_methods",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  const std::string path =
                      "/api/mixed/" + std::to_string(i);

                  switch (i % 5u)
                  {
                  case 0u:
                    app.get(path, text_handler);
                    break;

                  case 1u:
                    app.post(path, text_handler);
                    break;

                  case 2u:
                    app.put(path, text_handler);
                    break;

                  case 3u:
                    app.patch(path, text_handler);
                    break;

                  default:
                    app.del(path, text_handler);
                    break;
                  }
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/mixed/0"));
                assert(app.router()->has_route("POST", "/api/mixed/1"));
                assert(app.router()->has_route("PUT", "/api/mixed/2"));
                assert(app.router()->has_route("PATCH", "/api/mixed/3"));
                assert(app.router()->has_route("DELETE", "/api/mixed/4"));
                assert(app.router()->has_route("OPTIONS", "/api/mixed/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/mixed/0"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("POST", "/api/mixed/1"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("PUT", "/api/mixed/2"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("PATCH", "/api/mixed/3"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("DELETE", "/api/mixed/4"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_same_path_multiple_methods()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp * 5u;

    auto result =
        vix::bench::run(
            "app.route_registration/same_path_multiple_methods",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  const std::string path =
                      "/api/resources/" + std::to_string(i);

                  app.get(path, text_handler);
                  app.post(path, text_handler);
                  app.put(path, text_handler);
                  app.patch(path, text_handler);
                  app.del(path, text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/resources/42"));
                assert(app.router()->has_route("POST", "/api/resources/42"));
                assert(app.router()->has_route("PUT", "/api/resources/42"));
                assert(app.router()->has_route("PATCH", "/api/resources/42"));
                assert(app.router()->has_route("DELETE", "/api/resources/42"));
                assert(app.router()->has_route("OPTIONS", "/api/resources/42"));

                const auto &records =
                    app.router()->routes();

                assert(count_records(records, "OPTIONS", "/api/resources/42") == 1u);

                sink += records.size();
                sink += count_records(records, "OPTIONS", "/api/resources/42");
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/resources/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("DELETE", "/api/resources/42"));

                app.close();
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

  static vix::bench::BenchmarkResult bench_register_duplicate_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kDuplicateRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/duplicate_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kDuplicateRoutesPerApp; ++i)
                {
                  app.get(
                      "/api/duplicate",
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/duplicate"));
                assert(app.router()->has_route("OPTIONS", "/api/duplicate"));

                const auto &records =
                    app.router()->routes();

                assert(count_records(records, "GET", "/api/duplicate") == kDuplicateRoutesPerApp);
                assert(count_records(records, "OPTIONS", "/api/duplicate") == 1u);

                sink += records.size();
                sink += count_records(records, "GET", "/api/duplicate");
                sink += count_records(records, "OPTIONS", "/api/duplicate");

                app.close();
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

  static vix::bench::BenchmarkResult bench_register_paths_normalization()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/path_normalization",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  if ((i % 2u) == 0u)
                  {
                    app.get(
                        "api/no-leading-slash/" + std::to_string(i),
                        text_handler);
                  }
                  else
                  {
                    app.get(
                        "/api/trailing-slash/" + std::to_string(i) + "/",
                        text_handler);
                  }
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/no-leading-slash/0"));
                assert(app.router()->has_route("GET", "/api/trailing-slash/1"));
                assert(app.router()->has_route("GET", "/api/trailing-slash/1/"));
                assert(app.router()->has_route("OPTIONS", "/api/no-leading-slash/0"));
                assert(app.router()->has_route("OPTIONS", "/api/trailing-slash/1"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/no-leading-slash/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/trailing-slash/43"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/no-leading-slash/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/trailing-slash/43"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_register_mixed_route_tree()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerApp;

    auto result =
        vix::bench::run(
            "app.route_registration/mixed_route_tree",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
                {
                  switch (i % 5u)
                  {
                  case 0u:
                    app.get(
                        "/api/items/" + std::to_string(i),
                        text_handler);
                    break;

                  case 1u:
                    app.post(
                        "/api/items/" + std::to_string(i) + "/comments/{comment}",
                        text_handler);
                    break;

                  case 2u:
                    app.put(
                        "/api/orgs/{org}/projects/" + std::to_string(i),
                        text_handler);
                    break;

                  case 3u:
                    app.patch(
                        "/assets/v" + std::to_string(i) + "/app.js",
                        text_handler);
                    break;

                  default:
                    app.del(
                        "/health/" + std::to_string(i),
                        text_handler);
                    break;
                  }
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/items/0"));
                assert(app.router()->has_route("POST", "/api/items/1/comments/abc"));
                assert(app.router()->has_route("PUT", "/api/orgs/vix/projects/2"));
                assert(app.router()->has_route("PATCH", "/assets/v3/app.js"));
                assert(app.router()->has_route("DELETE", "/health/4"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/items/0"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("POST", "/api/items/1/comments/abc"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("PUT", "/api/orgs/vix/projects/2"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("PATCH", "/assets/v3/app.js"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("DELETE", "/health/4"));

                app.close();
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

  static vix::bench::BenchmarkResult bench_route_records_scan_after_registration()
  {
    prepare_app_env();

    App app;

    for (std::uint64_t i = 0; i < kRoutesPerApp; ++i)
    {
      app.get(
          "/api/records/" + std::to_string(i),
          text_handler);
    }

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/api/records/0"));
    assert(app.router()->has_route("GET", "/api/records/" + std::to_string(kRoutesPerApp - 1u)));

    const auto &records =
        app.router()->routes();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "app.route_registration/route_records_scan",
            kRecordScanIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kRecordScanIterations; ++i)
              {
                const std::string path =
                    "/api/records/" + std::to_string(i % kRoutesPerApp);

                const bool found =
                    has_record(
                        records,
                        "GET",
                        path);

                sink += static_cast<std::uint64_t>(found);
              }

              vix::bench::do_not_optimize(sink);
            });

    app.close();

    assert(sink > 0u);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_app_construct_close());

  results.push_back(bench_register_get_routes());
  results.push_back(bench_register_post_routes());
  results.push_back(bench_register_put_routes());

  results.push_back(bench_register_param_routes());
  results.push_back(bench_register_nested_param_routes());

  results.push_back(bench_register_mixed_methods());
  results.push_back(bench_register_same_path_multiple_methods());
  results.push_back(bench_register_duplicate_routes());

  results.push_back(bench_register_paths_normalization());
  results.push_back(bench_register_mixed_route_tree());

  results.push_back(bench_route_records_scan_after_registration());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.app.route_registration",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
