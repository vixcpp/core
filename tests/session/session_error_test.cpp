/**
 *
 * @file session_error_test.cpp
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
#include <stdexcept>
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

  class ThrowingHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &,
        Response &) override
    {
      throw std::runtime_error("handler failed");
      co_return;
    }
  };

  class InvalidStatusHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &,
        Response &res) override
    {
      res.set_status(999);
      res.set_body("invalid status body");
      co_return;
    }
  };

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
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

  static RawResponse run_and_parse(
      Router &router,
      const std::string &raw_request)
  {
    return parse_raw_response(
        run_session_once(router, raw_request));
  }

  static void assert_common_error_headers(const RawResponse &res)
  {
    assert(header_value(res, "Server") == "Vix.cpp");
    assert(!header_value(res, "Date").empty());
    assert(!header_value(res, "Content-Length").empty());
    assert(header_value(res, "Connection") == "close");
  }

  static void assert_json_message(
      const RawResponse &res,
      const std::string &expected)
  {
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

    const auto body = vix::json::loads(res.body);

    assert(body["message"].get<std::string>() == expected);
  }

  static void assert_malformed_request_response(const RawResponse &res)
  {
    assert(res.version == "HTTP/1.1");
    assert(res.status == vix::http::BAD_REQUEST);
    assert(res.reason == "Bad Request");
    assert_common_error_headers(res);
    assert_json_message(res, "Malformed HTTP request");
  }

  static void test_invalid_request_line_without_spaces_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "GET\r\n"
        "Host: localhost\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_invalid_request_line_missing_version_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "GET /missing-version\r\n"
        "Host: localhost\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_invalid_request_line_empty_target_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "GET  HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_unsupported_http_version_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "GET / FTP/1.0\r\n"
        "Host: localhost\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_malformed_header_without_colon_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "GET / HTTP/1.1\r\n"
        "Host localhost\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_empty_header_name_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "GET / HTTP/1.1\r\n"
        ": value\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_invalid_content_length_text_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: abc\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_invalid_content_length_negative_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: -1\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_invalid_content_length_with_suffix_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 12x\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_content_length_overflow_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 1844674407370955161518446744073709551615\r\n"
        "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_declared_payload_too_large_returns_400()
  {
    Router router;

    const std::size_t too_large =
        vix::session::MAX_REQUEST_BODY_SIZE + 1;

    const RawResponse res = run_and_parse(
        router,
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " +
            std::to_string(too_large) +
            "\r\n"
            "\r\n");

    assert_malformed_request_response(res);
  }

  static void test_incomplete_body_returns_400()
  {
    Router router;

    const RawResponse res = run_and_parse(
        router,
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "abc");

    assert_malformed_request_response(res);
  }

  static void test_header_too_large_returns_400()
  {
    Router router;

    std::string raw;

    raw += "GET / HTTP/1.1\r\n";
    raw += "Host: localhost\r\n";
    raw += "X-Large: ";
    raw += std::string(70 * 1024, 'a');
    raw += "\r\n";
    raw += "\r\n";

    const RawResponse res = run_and_parse(router, raw);

    assert_malformed_request_response(res);
  }

  static void test_handler_exception_returns_500()
  {
    Router router;

    router.add_route(
        "GET",
        "/throw",
        std::make_shared<ThrowingHandler>());

    const RawResponse res = run_and_parse(
        router,
        "GET /throw HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    assert(res.status == vix::http::INTERNAL_ERROR);
    assert(res.reason == "Internal Server Error");
    assert_common_error_headers(res);
    assert_json_message(res, "Internal server error");
  }

  static void test_invalid_status_from_handler_is_serialized_as_unknown()
  {
    Router router;

    router.add_route(
        "GET",
        "/invalid-status",
        std::make_shared<InvalidStatusHandler>());

    const RawResponse res = run_and_parse(
        router,
        "GET /invalid-status HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n");

    assert(res.status == 999);
    assert(res.reason == "Unknown");
    assert(res.body == "invalid status body");
    assert(header_value(res, "Connection") == "close");
  }

  static void test_empty_client_input_produces_no_response()
  {
    Router router;

    const std::string raw = run_session_once(router, "");

    assert(raw.empty());
  }

} // namespace

int main()
{
  test_invalid_request_line_without_spaces_returns_400();
  test_invalid_request_line_missing_version_returns_400();
  test_invalid_request_line_empty_target_returns_400();
  test_unsupported_http_version_returns_400();

  test_malformed_header_without_colon_returns_400();
  test_empty_header_name_returns_400();

  test_invalid_content_length_text_returns_400();
  test_invalid_content_length_negative_returns_400();
  test_invalid_content_length_with_suffix_returns_400();
  test_content_length_overflow_returns_400();
  test_declared_payload_too_large_returns_400();
  test_incomplete_body_returns_400();
  test_header_too_large_returns_400();

  test_handler_exception_returns_500();
  test_invalid_status_from_handler_is_serialized_as_unknown();

  test_empty_client_input_produces_no_response();

  return 0;
}
