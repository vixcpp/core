/**
 *
 * @file session_http_get_test.cpp
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
#include <cstring>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

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
  using Request = vix::http::Request;
  using Response = vix::http::Response;
  using Router = vix::router::Router;
  using Config = vix::config::Config;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;
  using Transport = vix::session::Transport;
  using Session = vix::session::Session;

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

      const std::size_t remaining = input_.size() - read_offset_;
      const std::size_t n = std::min(buffer.size(), remaining);

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

      const auto *data = reinterpret_cast<const char *>(buffer.data());

      output_.append(data, buffer.size());

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
      Response::text_response(res, body_, status_);
      co_return;
    }

  private:
    int status_;
    std::string body_;
  };

  class EchoPathHandler final : public vix::http::IRequestHandler
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
              {"query", req.query_string()},
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

  static vix::async::core::task<void> run_session_task(
      vix::async::core::io_context &ctx,
      std::shared_ptr<Session> session)
  {
    co_await session->run();

    ctx.stop();

    co_return;
  }

  static std::shared_ptr<vix::http::IRequestHandler> echo_path_handler()
  {
    return std::make_shared<EchoPathHandler>();
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
  }

  static bool contains(
      const std::string &text,
      const std::string &needle)
  {
    return text.find(needle) != std::string::npos;
  }

  static std::string header_value(
      const RawResponse &res,
      const std::string &name)
  {
    auto it = res.headers.find(name);
    return it == res.headers.end() ? std::string{} : it->second;
  }

  static RawResponse parse_raw_response(const std::string &raw)
  {
    RawResponse parsed;

    const std::size_t header_end = raw.find("\r\n\r\n");
    assert(header_end != std::string::npos);

    const std::string header_block = raw.substr(0, header_end);
    parsed.body = raw.substr(header_end + 4);

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

      const std::size_t colon = line.find(':');
      assert(colon != std::string::npos);

      std::string key = line.substr(0, colon);
      std::string value = line.substr(colon + 1);

      while (!value.empty() && value.front() == ' ')
      {
        value.erase(value.begin());
      }

      parsed.headers[std::move(key)] = std::move(value);
    }

    return parsed;
  }

  static std::string run_session_once(
      Router &router,
      const std::string &raw_request)
  {
    Config config;
    auto executor = make_executor();

    auto transport = std::make_unique<FakeTransport>(raw_request);
    FakeTransport *transport_ptr = transport.get();

    auto session = std::make_shared<Session>(
        std::move(transport),
        router,
        config,
        executor);

    vix::async::core::io_context ctx;

    auto task = run_session_task(ctx, session);

    std::move(task).start(ctx.get_scheduler());

    ctx.run();

    const std::string output = transport_ptr->output();

    assert(transport_ptr->close_count() >= 1);

    executor->stop();

    return output;
  }

  static void assert_common_response_headers(const RawResponse &res)
  {
    assert(header_value(res, "Server") == "Vix.cpp");
    assert(!header_value(res, "Date").empty());
    assert(!header_value(res, "Content-Length").empty());
    assert(!header_value(res, "Connection").empty());
  }

  static void test_get_static_route_returns_text_response()
  {
    Router router;

    router.add_route(
        "GET",
        "/health",
        text_handler(vix::http::OK, "healthy"));

    const std::string raw = run_session_once(
        router,
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.version == "HTTP/1.1");
    assert(res.status == vix::http::OK);
    assert(res.reason == "OK");
    assert(res.body == "healthy");

    assert(header_value(res, "Content-Type") == "text/plain; charset=utf-8");
    assert(header_value(res, "Content-Length") == "7");
    assert(header_value(res, "Connection") == "keep-alive");

    assert_common_response_headers(res);
  }

  static void test_get_root_route()
  {
    Router router;

    router.add_route(
        "GET",
        "/",
        text_handler(vix::http::OK, "root"));

    const std::string raw = run_session_once(
        router,
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::OK);
    assert(res.body == "root");
    assert(header_value(res, "Content-Length") == "4");
    assert(header_value(res, "Connection") == "keep-alive");
  }

  static void test_get_route_with_query_string()
  {
    Router router;

    router.add_route(
        "GET",
        "/echo",
        echo_path_handler());

    const std::string raw = run_session_once(
        router,
        "GET /echo?name=vix&page=2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::OK);
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

    const auto body = vix::json::loads(res.body);

    assert(body["method"].get<std::string>() == "GET");
    assert(body["target"].get<std::string>() == "/echo?name=vix&page=2");
    assert(body["path"].get<std::string>() == "/echo");
    assert(body["query"].get<std::string>() == "name=vix&page=2");
  }

  static void test_get_param_route()
  {
    Router router;

    router.add_route(
        "GET",
        "/users/{id}",
        echo_path_handler());

    const std::string raw = run_session_once(
        router,
        "GET /users/42 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::OK);

    const auto body = vix::json::loads(res.body);

    assert(body["method"].get<std::string>() == "GET");
    assert(body["target"].get<std::string>() == "/users/42");
    assert(body["path"].get<std::string>() == "/users/42");
  }

  static void test_get_method_is_matched_case_insensitively_by_router()
  {
    Router router;

    router.add_route(
        "GET",
        "/case",
        echo_path_handler());

    const std::string raw = run_session_once(
        router,
        "get /case HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::OK);

    const auto body = vix::json::loads(res.body);

    assert(body["method"].get<std::string>() == "get");
    assert(body["target"].get<std::string>() == "/case");
    assert(body["path"].get<std::string>() == "/case");
  }

  static void test_missing_route_returns_default_404_json()
  {
    Router router;

    const std::string raw = run_session_once(
        router,
        "GET /missing HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::NOT_FOUND);
    assert(res.reason == "Not Found");
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");
    assert(header_value(res, "Connection") == "close");

    const auto body = vix::json::loads(res.body);

    assert(body["error"].get<std::string>() == "Route not found");
    assert(body["method"].get<std::string>() == "GET");
    assert(body["path"].get<std::string>() == "/missing");
  }

  static void test_missing_route_with_query_keeps_target_in_404_body()
  {
    Router router;

    const std::string raw = run_session_once(
        router,
        "GET /missing?debug=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::NOT_FOUND);

    const auto body = vix::json::loads(res.body);

    assert(body["error"].get<std::string>() == "Route not found");
    assert(body["method"].get<std::string>() == "GET");
    assert(body["path"].get<std::string>() == "/missing?debug=1");
  }

  static void test_connection_close_request_closes_response()
  {
    Router router;

    router.add_route(
        "GET",
        "/close",
        text_handler(vix::http::OK, "bye"));

    const std::string raw = run_session_once(
        router,
        "GET /close HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n");

    const RawResponse res = parse_raw_response(raw);

    assert(res.status == vix::http::OK);
    assert(res.body == "bye");
    assert(header_value(res, "Connection") == "close");
    assert(header_value(res, "Content-Length") == "3");
  }

  static void test_raw_response_contains_single_header_body_separator()
  {
    Router router;

    router.add_route(
        "GET",
        "/format",
        text_handler(vix::http::OK, "ok"));

    const std::string raw = run_session_once(
        router,
        "GET /format HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    assert(contains(raw, "\r\n\r\nok"));

    const std::size_t first = raw.find("\r\n\r\n");
    assert(first != std::string::npos);

    const std::size_t second = raw.find("\r\n\r\n", first + 4);
    assert(second == std::string::npos);
  }

} // namespace

int main()
{
  test_get_static_route_returns_text_response();
  test_get_root_route();
  test_get_route_with_query_string();
  test_get_param_route();
  test_get_method_is_matched_case_insensitively_by_router();

  test_missing_route_returns_default_404_json();
  test_missing_route_with_query_keeps_target_in_404_body();

  test_connection_close_request_closes_response();
  test_raw_response_contains_single_header_body_separator();

  return 0;
}
