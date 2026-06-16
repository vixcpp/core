/**
 *
 * @file session_fake_transport_bench.cpp
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
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/io_context.hpp>
#include <vix/async/core/task.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/json.hpp>
#include <vix/router/Router.hpp>
#include <vix/session/Session.hpp>
#include <vix/session/Transport.hpp>

namespace
{
  using Config = vix::config::Config;
  using Request = vix::http::Request;
  using Response = vix::http::Response;
  using Router = vix::router::Router;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;
  using Session = vix::session::Session;
  using Transport = vix::session::Transport;

  constexpr std::uint64_t kSingleRequestIterations = 5'000;
  constexpr std::uint64_t kPostRequestIterations = 3'000;
  constexpr std::uint64_t kPipelineIterations = 1'000;
  constexpr int kPipelineRequestCount = 20;

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

  static void prepare_session_env()
  {
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("SERVER_PORT", "18150");
    set_env_var("SERVER_IO_THREADS", "1");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("LOGGING_ASYNC", "false");
    set_env_var("WAF_MODE", "off");
  }

  class FakeTransport final : public Transport
  {
  public:
    explicit FakeTransport(std::string input)
        : input_(std::move(input))
    {
    }

    vix::async::core::task<std::size_t> async_read(
        std::span<std::byte> buffer,
        vix::async::core::cancel_token token) override
    {
      if (token.is_cancelled() || !open_ || buffer.empty())
      {
        co_return 0;
      }

      if (read_offset_ >= input_.size())
      {
        co_return 0;
      }

      const std::size_t remaining =
          input_.size() - read_offset_;

      const std::size_t n =
          std::min(buffer.size(), remaining);

      std::memcpy(
          buffer.data(),
          input_.data() + read_offset_,
          n);

      read_offset_ += n;

      co_return n;
    }

    vix::async::core::task<std::size_t> async_write(
        std::span<const std::byte> buffer,
        vix::async::core::cancel_token token) override
    {
      if (token.is_cancelled() || !open_ || buffer.empty())
      {
        co_return 0;
      }

      const auto *data =
          reinterpret_cast<const char *>(buffer.data());

      output_.append(
          data,
          buffer.size());

      co_return buffer.size();
    }

    [[nodiscard]] bool is_open() const noexcept override
    {
      return open_;
    }

    void close() noexcept override
    {
      open_ = false;
      ++close_count_;
    }

    [[nodiscard]] const std::string &output() const noexcept
    {
      return output_;
    }

    [[nodiscard]] int close_count() const noexcept
    {
      return close_count_;
    }

  private:
    std::string input_{};
    std::string output_{};
    std::size_t read_offset_{0};
    bool open_{true};
    int close_count_{0};
  };

  struct RawResponse
  {
    std::string version{};
    int status{0};
    std::string reason{};
    std::map<std::string, std::string> headers{};
    std::string body{};
  };

  class TextHandler final : public vix::http::IRequestHandler
  {
  public:
    TextHandler(
        int status,
        std::string body)
        : status_(status),
          body_(std::move(body))
    {
    }

    vix::async::core::task<void> handle_request(
        const Request &,
        Response &res) override
    {
      Response::text_response(
          res,
          body_,
          status_);

      co_return;
    }

  private:
    int status_;
    std::string body_;
  };

  class JsonEchoHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) override
    {
      vix::json::Json body =
          vix::json::Json::object();

      if (!req.body().empty())
      {
        body = req.json();
      }

      Response::json_response(
          res,
          vix::json::Json{
              {"method", req.method()},
              {"target", req.target()},
              {"path", req.path()},
              {"query", req.query_string()},
              {"body_size", req.body().size()},
              {"payload", body},
          },
          vix::http::OK);

      co_return;
    }
  };

  class ParamEchoHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) override
    {
      Response::json_response(
          res,
          vix::json::Json{
              {"method", req.method()},
              {"target", req.target()},
              {"path", req.path()},
          },
          vix::http::OK);

      co_return;
    }
  };

  static std::shared_ptr<vix::http::IRequestHandler> text_handler(
      int status,
      std::string body)
  {
    return std::make_shared<TextHandler>(
        status,
        std::move(body));
  }

  static std::shared_ptr<vix::http::IRequestHandler> json_echo_handler()
  {
    return std::make_shared<JsonEchoHandler>();
  }

  static std::shared_ptr<vix::http::IRequestHandler> param_echo_handler()
  {
    return std::make_shared<ParamEchoHandler>();
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    auto executor =
        std::make_shared<RuntimeExecutor>(1u);

    executor->start();

    return executor;
  }

  static Config make_config()
  {
    prepare_session_env();

    Config config{};

    assert(config.isTlsEnabled() == false);
    assert(config.getIOThreads() == 1);
    assert(config.getWafMode() == "off");

    return config;
  }

  static void install_routes(Router &router)
  {
    router.add_route(
        "GET",
        "/health",
        text_handler(
            vix::http::OK,
            "healthy"));

    router.add_route(
        "GET",
        "/",
        text_handler(
            vix::http::OK,
            "root"));

    router.add_route(
        "GET",
        "/api/status",
        text_handler(
            vix::http::OK,
            "status"));

    router.add_route(
        "GET",
        "/api/users/{id}",
        param_echo_handler());

    router.add_route(
        "GET",
        "/api/search",
        json_echo_handler());

    router.add_route(
        "POST",
        "/api/items",
        json_echo_handler());

    router.add_route(
        "PUT",
        "/api/items/{id}",
        json_echo_handler());
  }

  static vix::async::core::task<void> run_session_task(
      vix::async::core::io_context &ctx,
      std::shared_ptr<Session> session)
  {
    co_await session->run();

    ctx.stop();

    co_return;
  }

  static std::string run_session_once(
      Router &router,
      const Config &config,
      const std::shared_ptr<RuntimeExecutor> &executor,
      const std::string &raw_request)
  {
    auto transport =
        std::make_unique<FakeTransport>(raw_request);

    FakeTransport *transport_ptr =
        transport.get();

    auto session =
        std::make_shared<Session>(
            std::move(transport),
            router,
            config,
            executor);

    vix::async::core::io_context ctx;

    auto task =
        run_session_task(
            ctx,
            session);

    std::move(task).start(ctx.get_scheduler());

    ctx.run();

    const std::string output =
        transport_ptr->output();

    assert(transport_ptr->close_count() >= 1);

    return output;
  }

  static std::string header_value(
      const RawResponse &res,
      const std::string &name)
  {
    auto it =
        res.headers.find(name);

    return it == res.headers.end()
               ? std::string{}
               : it->second;
  }

  static RawResponse parse_first_response(
      const std::string &raw)
  {
    RawResponse parsed;

    const std::size_t header_end =
        raw.find("\r\n\r\n");

    assert(header_end != std::string::npos);

    const std::string header_block =
        raw.substr(0, header_end);

    parsed.body =
        raw.substr(header_end + 4);

    std::istringstream input(header_block);

    std::string status_line;
    std::getline(input, status_line);

    if (!status_line.empty() && status_line.back() == '\r')
    {
      status_line.pop_back();
    }

    {
      std::istringstream line(status_line);

      line >> parsed.version;
      line >> parsed.status;

      std::getline(line, parsed.reason);

      if (!parsed.reason.empty() && parsed.reason.front() == ' ')
      {
        parsed.reason.erase(parsed.reason.begin());
      }
    }

    std::string line;

    while (std::getline(input, line))
    {
      if (!line.empty() && line.back() == '\r')
      {
        line.pop_back();
      }

      const std::size_t colon =
          line.find(':');

      assert(colon != std::string::npos);

      std::string key =
          line.substr(0, colon);

      std::string value =
          line.substr(colon + 1);

      while (!value.empty() && value.front() == ' ')
      {
        value.erase(value.begin());
      }

      parsed.headers[std::move(key)] =
          std::move(value);
    }

    return parsed;
  }

  static std::size_t count_http_responses(
      std::string_view raw)
  {
    std::size_t count = 0;
    std::size_t pos = 0;

    while (true)
    {
      pos =
          raw.find(
              "HTTP/1.1 ",
              pos);

      if (pos == std::string_view::npos)
      {
        break;
      }

      ++count;
      pos += 9;
    }

    return count;
  }

  static std::string make_get_health_request()
  {
    return "GET /health HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "\r\n";
  }

  static std::string make_get_root_request()
  {
    return "GET / HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "\r\n";
  }

  static std::string make_get_query_request()
  {
    return "GET /api/search?q=vix&page=2&limit=25 HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Accept: application/json\r\n"
           "\r\n";
  }

  static std::string make_get_param_request()
  {
    return "GET /api/users/42 HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Accept: application/json\r\n"
           "\r\n";
  }

  static std::string make_missing_request()
  {
    return "GET /missing HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "\r\n";
  }

  static std::string make_connection_close_request()
  {
    return "GET /health HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Connection: close\r\n"
           "\r\n";
  }

  static std::string make_post_json_request()
  {
    const std::string body =
        R"({"name":"Vix.cpp","version":"2.6.3","active":true,"count":42})";

    return "POST /api/items HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " +
           std::to_string(body.size()) +
           "\r\n"
           "\r\n" +
           body;
  }

  static std::string make_put_json_request()
  {
    const std::string body =
        R"({"name":"Updated","active":true})";

    return "PUT /api/items/42 HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " +
           std::to_string(body.size()) +
           "\r\n"
           "\r\n" +
           body;
  }

  static std::string make_pipeline_request()
  {
    std::string raw;

    raw.reserve(2048);

    for (int i = 0; i < kPipelineRequestCount; ++i)
    {
      raw +=
          "GET /health HTTP/1.1\r\n"
          "Host: localhost\r\n";

      if (i + 1 == kPipelineRequestCount)
      {
        raw += "Connection: close\r\n";
      }

      raw += "\r\n";
    }

    return raw;
  }

  static vix::bench::BenchmarkResult bench_single_get_health()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_get_health_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/single_get_health",
            kSingleRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSingleRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(res.body == "healthy");
                assert(header_value(res, "Content-Length") == "7");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_single_get_root()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_get_root_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/single_get_root",
            kSingleRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSingleRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(res.body == "root");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_get_query_route()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_get_query_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/get_query_route",
            kSingleRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSingleRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_get_param_route()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_get_param_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/get_param_route",
            kSingleRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSingleRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_missing_route_404()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_missing_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/missing_route_404",
            kSingleRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSingleRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::NOT_FOUND);
                assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_connection_close()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_connection_close_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/connection_close",
            kSingleRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kSingleRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(header_value(res, "Connection") == "close");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_post_json_body()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_post_json_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/post_json_body",
            kPostRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kPostRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_put_json_body_param_route()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_put_json_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/put_json_body_param_route",
            kPostRequestIterations,
            [&]()
            {
              for (std::uint64_t i = 0; i < kPostRequestIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const RawResponse res =
                    parse_first_response(output);

                assert(res.status == vix::http::OK);
                assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

                sink += output.size();
                sink += static_cast<std::uint64_t>(res.status);
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 2,
                .measure_iterations = 9,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

  static vix::bench::BenchmarkResult bench_keep_alive_pipeline()
  {
    Router router;
    install_routes(router);

    Config config =
        make_config();

    auto executor =
        make_executor();

    const std::string request =
        make_pipeline_request();

    std::uint64_t sink = 0;

    auto result =
        vix::bench::run(
            "session.fake_transport/keep_alive_pipeline",
            kPipelineIterations * static_cast<std::uint64_t>(kPipelineRequestCount),
            [&]()
            {
              for (std::uint64_t i = 0; i < kPipelineIterations; ++i)
              {
                const std::string output =
                    run_session_once(
                        router,
                        config,
                        executor,
                        request);

                const std::size_t response_count =
                    count_http_responses(output);

                assert(response_count == static_cast<std::size_t>(kPipelineRequestCount));

                sink += output.size();
                sink += response_count;
              }

              vix::bench::do_not_optimize(sink);
            },
            vix::bench::BenchmarkConfig{
                .warmup_iterations = 1,
                .measure_iterations = 7,
            });

    executor->stop();

    assert(sink > 0u);

    return result;
  }

} // namespace

int main(int argc, char **argv)
{
  std::vector<vix::bench::BenchmarkResult> results;

  results.push_back(bench_single_get_health());
  results.push_back(bench_single_get_root());
  results.push_back(bench_get_query_route());
  results.push_back(bench_get_param_route());
  results.push_back(bench_missing_route_404());
  results.push_back(bench_connection_close());
  results.push_back(bench_post_json_body());
  results.push_back(bench_put_json_body_param_route());
  results.push_back(bench_keep_alive_pipeline());

  vix::bench::print_results(results);

  if (argc > 1)
  {
    vix::bench::write_report_json(
        argv[1],
        "vix.core.session.fake_transport",
        vix::bench::env_or_default("VIX_BENCH_VERSION", "dev"),
        results);
  }

  return EXIT_SUCCESS;
}
