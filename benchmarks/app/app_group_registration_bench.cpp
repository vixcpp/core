/**
 *
 * @file app_group_registration_bench.cpp
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

  constexpr std::uint64_t kGroupsPerApp = 100;
  constexpr std::uint64_t kRoutesPerGroup = 20;
  constexpr std::uint64_t kRegistrationIterations = 50;
  constexpr std::uint64_t kNestedGroupIterations = 50;
  constexpr std::uint64_t kManyRoutesPerGroup = 500;
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

  static vix::bench::BenchmarkResult bench_create_group_objects()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kGroupsPerApp;

    auto result =
        vix::bench::run(
            "app.group_registration/create_group_objects",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kGroupsPerApp; ++i)
                {
                  auto group =
                      app.group(
                          "/api/v" + std::to_string(i));

                  group.get(
                      "/status",
                      text_handler);

                  sink += static_cast<std::uint64_t>(
                      app.router()->has_route(
                          "GET",
                          "/api/v" + std::to_string(i) + "/status"));
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v0/status"));
                assert(app.router()->has_route("OPTIONS", "/api/v0/status"));
                assert(app.router()->has_route("GET", "/api/v" + std::to_string(kGroupsPerApp - 1u) + "/status"));

                sink += app.router()->routes().size();

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

  static vix::bench::BenchmarkResult bench_callback_group_get_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/callback_get_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                app.group(
                    "/api",
                    [](App::Group api)
                    {
                      for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                      {
                        api.get(
                            "/items/" + std::to_string(i),
                            text_handler);
                      }
                    });

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/items/0"));
                assert(app.router()->has_route("GET", "/api/items/" + std::to_string(kRoutesPerGroup - 1u)));
                assert(app.router()->has_route("OPTIONS", "/api/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/items/7"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/items/7"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_returned_group_get_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/returned_group_get_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                {
                  api.get(
                      "/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/items/0"));
                assert(app.router()->has_route("GET", "/api/items/" + std::to_string(kManyRoutesPerGroup - 1u)));
                assert(app.router()->has_route("OPTIONS", "/api/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/items/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/items/42"));

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

  static vix::bench::BenchmarkResult bench_group_post_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/post_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                {
                  api.post(
                      "/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("POST", "/api/items/0"));
                assert(app.router()->has_route("POST", "/api/items/" + std::to_string(kManyRoutesPerGroup - 1u)));
                assert(app.router()->has_route("OPTIONS", "/api/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("POST", "/api/items/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/items/42"));

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

  static vix::bench::BenchmarkResult bench_group_mixed_methods()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/mixed_methods",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                {
                  const std::string path =
                      "/resources/" + std::to_string(i);

                  switch (i % 5u)
                  {
                  case 0u:
                    api.get(path, text_handler);
                    break;

                  case 1u:
                    api.post(path, text_handler);
                    break;

                  case 2u:
                    api.put(path, text_handler);
                    break;

                  case 3u:
                    api.patch(path, text_handler);
                    break;

                  default:
                    api.del(path, text_handler);
                    break;
                  }
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/resources/0"));
                assert(app.router()->has_route("POST", "/api/resources/1"));
                assert(app.router()->has_route("PUT", "/api/resources/2"));
                assert(app.router()->has_route("PATCH", "/api/resources/3"));
                assert(app.router()->has_route("DELETE", "/api/resources/4"));
                assert(app.router()->has_route("OPTIONS", "/api/resources/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/resources/0"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("POST", "/api/resources/1"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("PUT", "/api/resources/2"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("PATCH", "/api/resources/3"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("DELETE", "/api/resources/4"));

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

  static vix::bench::BenchmarkResult bench_group_same_path_multiple_methods()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerGroup * 5u;

    auto result =
        vix::bench::run(
            "app.group_registration/same_path_multiple_methods",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                {
                  const std::string path =
                      "/resources/" + std::to_string(i);

                  api.get(path, text_handler);
                  api.post(path, text_handler);
                  api.put(path, text_handler);
                  api.patch(path, text_handler);
                  api.del(path, text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/resources/7"));
                assert(app.router()->has_route("POST", "/api/resources/7"));
                assert(app.router()->has_route("PUT", "/api/resources/7"));
                assert(app.router()->has_route("PATCH", "/api/resources/7"));
                assert(app.router()->has_route("DELETE", "/api/resources/7"));
                assert(app.router()->has_route("OPTIONS", "/api/resources/7"));

                const auto &records =
                    app.router()->routes();

                assert(count_records(records, "OPTIONS", "/api/resources/7") == 1u);

                sink += records.size();
                sink += count_records(records, "OPTIONS", "/api/resources/7");
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/resources/7"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("DELETE", "/api/resources/7"));

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

  static vix::bench::BenchmarkResult bench_group_param_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/param_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                {
                  api.get(
                      "/v" + std::to_string(i) + "/users/{id}",
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v0/users/42"));
                assert(app.router()->has_route("GET", "/api/v" + std::to_string(kManyRoutesPerGroup - 1u) + "/users/42"));
                assert(app.router()->has_route("OPTIONS", "/api/v0/users/42"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/v42/users/gaspard"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/v42/users/gaspard"));

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

  static vix::bench::BenchmarkResult bench_nested_group_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kNestedGroupIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/nested_group_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kNestedGroupIterations; ++iteration)
              {
                App app;

                app.group(
                    "/api",
                    [](App::Group api)
                    {
                      api.group(
                          "/v1",
                          [](App::Group v1)
                          {
                            for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                            {
                              v1.get(
                                  "/items/" + std::to_string(i),
                                  text_handler);
                            }
                          });
                    });

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v1/items/0"));
                assert(app.router()->has_route("GET", "/api/v1/items/" + std::to_string(kManyRoutesPerGroup - 1u)));
                assert(app.router()->has_route("OPTIONS", "/api/v1/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/v1/items/42"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/v1/items/42"));

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

  static vix::bench::BenchmarkResult bench_deeply_nested_group_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kNestedGroupIterations * kRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/deeply_nested_group_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kNestedGroupIterations; ++iteration)
              {
                App app;

                app.group(
                    "/api",
                    [](App::Group api)
                    {
                      api.group(
                          "/v1",
                          [](App::Group v1)
                          {
                            v1.group(
                                "/admin",
                                [](App::Group admin)
                                {
                                  admin.group(
                                      "/reports",
                                      [](App::Group reports)
                                      {
                                        for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                                        {
                                          reports.get(
                                              "/items/" + std::to_string(i) + "/{id}",
                                              text_handler);
                                        }
                                      });
                                });
                          });
                    });

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v1/admin/reports/items/0/42"));
                assert(app.router()->has_route("GET", "/api/v1/admin/reports/items/" + std::to_string(kRoutesPerGroup - 1u) + "/42"));
                assert(app.router()->has_route("OPTIONS", "/api/v1/admin/reports/items/0/42"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/v1/admin/reports/items/7/gaspard"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/v1/admin/reports/items/7/gaspard"));

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

  static vix::bench::BenchmarkResult bench_nested_group_param_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kNestedGroupIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/nested_group_param_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kNestedGroupIterations; ++iteration)
              {
                App app;

                app.group(
                    "/api",
                    [](App::Group api)
                    {
                      api.group(
                          "/orgs/{org}",
                          [](App::Group org)
                          {
                            for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                            {
                              org.get(
                                  "/v" + std::to_string(i) + "/users/{user}",
                                  text_handler);
                            }
                          });
                    });

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/orgs/vix/v0/users/gaspard"));
                assert(app.router()->has_route("GET", "/api/orgs/vix/v" + std::to_string(kManyRoutesPerGroup - 1u) + "/users/gaspard"));
                assert(app.router()->has_route("OPTIONS", "/api/orgs/vix/v0/users/gaspard"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/orgs/vix/v42/users/gaspard"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/orgs/vix/v42/users/gaspard"));

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

  static vix::bench::BenchmarkResult bench_group_path_normalization()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/path_normalization",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                app.group(
                    "api/",
                    [](App::Group api)
                    {
                      for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                      {
                        if ((i % 2u) == 0u)
                        {
                          api.get(
                              "items/" + std::to_string(i),
                              text_handler);
                        }
                        else
                        {
                          api.get(
                              "/items/" + std::to_string(i) + "/",
                              text_handler);
                        }
                      }
                    });

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/items/0"));
                assert(app.router()->has_route("GET", "/api/items/1"));
                assert(app.router()->has_route("GET", "/api/items/1/"));
                assert(app.router()->has_route("OPTIONS", "/api/items/0"));
                assert(app.router()->has_route("OPTIONS", "/api/items/1"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/items/0"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/items/1"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/items/0"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/items/1"));

                app.close();
              }

              vix::bench::do_not_optimize(sink);
            });

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_group_duplicate_routes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kManyRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/duplicate_routes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
                {
                  api.get(
                      "/duplicate",
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/duplicate"));
                assert(app.router()->has_route("OPTIONS", "/api/duplicate"));

                const auto &records =
                    app.router()->routes();

                assert(count_records(records, "GET", "/api/duplicate") == kManyRoutesPerGroup);
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

  static vix::bench::BenchmarkResult bench_multiple_group_objects_same_prefix()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/multiple_group_objects_same_prefix",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto first =
                    app.group("/api");

                auto second =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                {
                  first.get(
                      "/first/" + std::to_string(i),
                      text_handler);

                  second.get(
                      "/second/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/first/0"));
                assert(app.router()->has_route("GET", "/api/second/0"));
                assert(app.router()->has_route("OPTIONS", "/api/first/0"));
                assert(app.router()->has_route("OPTIONS", "/api/second/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/first/7"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/second/7"));

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

  static vix::bench::BenchmarkResult bench_multiple_groups_different_prefixes()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kGroupsPerApp;

    auto result =
        vix::bench::run(
            "app.group_registration/multiple_groups_different_prefixes",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                for (std::uint64_t i = 0; i < kGroupsPerApp; ++i)
                {
                  auto group =
                      app.group(
                          "/api/v" + std::to_string(i));

                  group.get(
                      "/status",
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/v0/status"));
                assert(app.router()->has_route("GET", "/api/v" + std::to_string(kGroupsPerApp - 1u) + "/status"));
                assert(app.router()->has_route("OPTIONS", "/api/v0/status"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/v42/status"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/v42/status"));

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

  static vix::bench::BenchmarkResult bench_group_middleware_helpers()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/middleware_helpers",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                auto api =
                    app.group("/api");

                api.use(
                       [](Request &, ResponseWrapper &, App::Next next)
                       {
                         next();
                       })
                    .protect(
                        "/private",
                        [](Request &, ResponseWrapper &, App::Next next)
                        {
                          next();
                        })
                    .protect_exact(
                        "/private/status",
                        [](Request &, ResponseWrapper &, App::Next next)
                        {
                          next();
                        });

                for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                {
                  api.get(
                      "/private/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/private/items/0"));
                assert(app.router()->has_route("GET", "/api/private/items/" + std::to_string(kRoutesPerGroup - 1u)));
                assert(app.router()->has_route("OPTIONS", "/api/private/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/private/items/7"));
                sink += static_cast<std::uint64_t>(app.router()->has_route("OPTIONS", "/api/private/items/7"));

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

  static vix::bench::BenchmarkResult bench_group_routes_after_templates()
  {
    prepare_app_env();

    std::uint64_t sink = 0;

    constexpr std::uint64_t operations =
        kRegistrationIterations * kRoutesPerGroup;

    auto result =
        vix::bench::run(
            "app.group_registration/routes_after_templates",
            operations,
            [&]()
            {
              for (std::uint64_t iteration = 0; iteration < kRegistrationIterations; ++iteration)
              {
                App app;

                app.templates("views");

                assert(app.has_views());

                auto api =
                    app.group("/api");

                for (std::uint64_t i = 0; i < kRoutesPerGroup; ++i)
                {
                  api.get(
                      "/items/" + std::to_string(i),
                      text_handler);
                }

                assert(app.router() != nullptr);
                assert(app.router()->has_route("GET", "/api/items/0"));
                assert(app.router()->has_route("OPTIONS", "/api/items/0"));

                sink += app.router()->routes().size();
                sink += static_cast<std::uint64_t>(app.has_views());
                sink += static_cast<std::uint64_t>(app.router()->has_route("GET", "/api/items/7"));

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

  static vix::bench::BenchmarkResult bench_route_records_scan_after_group_registration()
  {
    prepare_app_env();

    App app;

    auto api =
        app.group("/api");

    for (std::uint64_t i = 0; i < kManyRoutesPerGroup; ++i)
    {
      api.get(
          "/records/" + std::to_string(i),
          text_handler);
    }

    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/api/records/0"));
    assert(app.router()->has_route("GET", "/api/records/" + std::to_string(kManyRoutesPerGroup - 1u)));

    const auto &records =
        app.router()->routes();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "app.group_registration/route_records_scan",
            kRecordScanIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kRecordScanIterations; ++i)
              {
                const std::string path =
                    "/api/records/" + std::to_string(i % kManyRoutesPerGroup);

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

  results.push_back(bench_create_group_objects());

  results.push_back(bench_callback_group_get_routes());
  results.push_back(bench_returned_group_get_routes());
  results.push_back(bench_group_post_routes());
  results.push_back(bench_group_mixed_methods());
  results.push_back(bench_group_same_path_multiple_methods());

  results.push_back(bench_group_param_routes());

  results.push_back(bench_nested_group_routes());
  results.push_back(bench_deeply_nested_group_routes());
  results.push_back(bench_nested_group_param_routes());

  results.push_back(bench_group_path_normalization());
  results.push_back(bench_group_duplicate_routes());

  results.push_back(bench_multiple_group_objects_same_prefix());
  results.push_back(bench_multiple_groups_different_prefixes());

  results.push_back(bench_group_middleware_helpers());
  results.push_back(bench_group_routes_after_templates());

  results.push_back(bench_route_records_scan_after_group_registration());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.app.group_registration",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
