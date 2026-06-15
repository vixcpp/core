/**
 *
 * @file session_keep_alive_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <algorithm>
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
    explicit FakeTransport(
        std::string input,
        std::size_t max_read_chunk = static_cast<std::size_t>(-1))
        : input_(std::move(input)),
          max_read_chunk_(max_read_chunk)
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
      const std::size_t limit = std::min(buffer.size(), max_read_chunk_);
      const std::size_t n = std::min(limit, remaining);

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
    std::size_t max_read_chunk_{static_cast<std::size_t>(-1)};
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
    explicit TextHandler(std::string body)
        : body_(std::move(body))
    {
    }

    vix::async::core::task<void> handle_request(
        const Request &,
        Response &res) override
    {
      Response::text_response(res, body_, vix::http::OK);
      co_return;
    }

  private:
    std::string body_;
  };

  class EchoRequestHandler final : public vix::http::IRequestHandler
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
              {"connection", req.header("Connection")},
          },
          vix::http::OK);

      co_return;
    }
  };

  static std::shared_ptr<vix::http::IRequestHandler> text_handler(std::string body)
  {
    return std::make_shared<TextHandler>(std::move(body));
  }

  static std::shared_ptr<vix::http::IRequestHandler> echo_request_handler()
  {
    return std::make_shared<EchoRequestHandler>();
  }

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

  static vix::async::core::task<void> run_session_task(
      vix::async::core::io_context &ctx,
      std::shared_ptr<Session> session)
  {
    co_await session->run();

    ctx.stop();

    co_return;
  }

  static RawResponse parse_one_response(
      const std::string &raw,
      std::size_t &offset)
  {
    RawResponse parsed;

    const std::size_t header_end = raw.find("\r\n\r\n", offset);
    assert(header_end != std::string::npos);

    const std::string header_block = raw.substr(offset, header_end - offset);

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

    const std::string len_value = header_value(parsed, "Content-Length");
    const std::size_t content_length =
        len_value.empty() ? 0 : static_cast<std::size_t>(std::stoull(len_value));

    const std::size_t body_start = header_end + 4;

    assert(body_start + content_length <= raw.size());

    parsed.body = raw.substr(body_start, content_length);
    offset = body_start + content_length;

    return parsed;
  }

  static RawResponse parse_header_only_response(const std::string &raw)
  {
    RawResponse parsed;

    const std::size_t header_end = raw.find("\r\n\r\n");
    assert(header_end != std::string::npos);

    const std::string header_block = raw.substr(0, header_end);

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

    parsed.body.clear();

    return parsed;
  }

  static std::vector<RawResponse> parse_all_responses(const std::string &raw)
  {
    std::vector<RawResponse> responses;

    std::size_t offset = 0;

    while (offset < raw.size())
    {
      responses.push_back(parse_one_response(raw, offset));
    }

    return responses;
  }

  static std::string run_session(
      Router &router,
      const std::string &raw_request,
      std::size_t max_read_chunk = static_cast<std::size_t>(-1))
  {
    Config config;
    auto executor = make_executor();

    auto transport = std::make_unique<FakeTransport>(
        raw_request,
        max_read_chunk);

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

  static std::string get_request(
      std::string target,
      std::string connection = "")
  {
    std::string raw;

    raw += "GET ";
    raw += target;
    raw += " HTTP/1.1\r\n";
    raw += "Host: localhost\r\n";

    if (!connection.empty())
    {
      raw += "Connection: ";
      raw += connection;
      raw += "\r\n";
    }

    raw += "\r\n";

    return raw;
  }

  static std::string head_request(
      std::string target,
      std::string connection = "")
  {
    std::string raw;

    raw += "HEAD ";
    raw += target;
    raw += " HTTP/1.1\r\n";
    raw += "Host: localhost\r\n";

    if (!connection.empty())
    {
      raw += "Connection: ";
      raw += connection;
      raw += "\r\n";
    }

    raw += "\r\n";

    return raw;
  }

  static void assert_common_headers(const RawResponse &res)
  {
    assert(header_value(res, "Server") == "Vix.cpp");
    assert(!header_value(res, "Date").empty());
    assert(!header_value(res, "Content-Length").empty());
    assert(!header_value(res, "Connection").empty());
  }

  static void test_http_11_defaults_to_keep_alive()
  {
    Router router;

    router.add_route(
        "GET",
        "/one",
        text_handler("one"));

    const auto responses = parse_all_responses(
        run_session(router, get_request("/one")));

    assert(responses.size() == 1);

    const auto &res = responses[0];

    assert(res.status == vix::http::OK);
    assert(res.body == "one");
    assert(header_value(res, "Connection") == "keep-alive");
    assert(header_value(res, "Content-Length") == "3");
    assert_common_headers(res);
  }

  static void test_explicit_keep_alive_is_preserved()
  {
    Router router;

    router.add_route(
        "GET",
        "/keep",
        echo_request_handler());

    const auto responses = parse_all_responses(
        run_session(router, get_request("/keep", "keep-alive")));

    assert(responses.size() == 1);

    const auto &res = responses[0];

    assert(res.status == vix::http::OK);
    assert(header_value(res, "Connection") == "keep-alive");

    const auto body = vix::json::loads(res.body);

    assert(body["method"].get<std::string>() == "GET");
    assert(body["target"].get<std::string>() == "/keep");
    assert(body["connection"].get<std::string>() == "keep-alive");
  }

  static void test_explicit_connection_close_is_preserved()
  {
    Router router;

    router.add_route(
        "GET",
        "/close",
        echo_request_handler());

    const auto responses = parse_all_responses(
        run_session(router, get_request("/close", "close")));

    assert(responses.size() == 1);

    const auto &res = responses[0];

    assert(res.status == vix::http::OK);
    assert(header_value(res, "Connection") == "close");

    const auto body = vix::json::loads(res.body);

    assert(body["method"].get<std::string>() == "GET");
    assert(body["target"].get<std::string>() == "/close");
    assert(body["connection"].get<std::string>() == "close");
  }

  static void test_connection_close_is_case_insensitive_for_shutdown()
  {
    Router router;

    router.add_route(
        "GET",
        "/close",
        text_handler("closed"));

    const auto responses = parse_all_responses(
        run_session(router, get_request("/close", "Close")));

    assert(responses.size() == 1);

    const auto &res = responses[0];

    assert(res.status == vix::http::OK);
    assert(res.body == "closed");
    assert(header_value(res, "Connection") == "close");
  }

  static void test_multiple_keep_alive_requests_on_same_connection()
  {
    Router router;

    router.add_route(
        "GET",
        "/one",
        text_handler("one"));

    router.add_route(
        "GET",
        "/two",
        text_handler("two"));

    router.add_route(
        "GET",
        "/three",
        text_handler("three"));

    const std::string raw =
        get_request("/one", "keep-alive") +
        get_request("/two", "keep-alive") +
        get_request("/three", "close");

    const auto responses = parse_all_responses(
        run_session(router, raw));

    assert(responses.size() == 3);

    assert(responses[0].status == vix::http::OK);
    assert(responses[0].body == "one");
    assert(header_value(responses[0], "Connection") == "keep-alive");

    assert(responses[1].status == vix::http::OK);
    assert(responses[1].body == "two");
    assert(header_value(responses[1], "Connection") == "keep-alive");

    assert(responses[2].status == vix::http::OK);
    assert(responses[2].body == "three");
    assert(header_value(responses[2], "Connection") == "close");
  }

  static void test_connection_close_stops_processing_later_pipelined_requests()
  {
    Router router;

    router.add_route(
        "GET",
        "/first",
        text_handler("first"));

    router.add_route(
        "GET",
        "/second",
        text_handler("second"));

    const std::string raw =
        get_request("/first", "close") +
        get_request("/second", "keep-alive");

    const auto responses = parse_all_responses(
        run_session(router, raw));

    assert(responses.size() == 1);

    assert(responses[0].status == vix::http::OK);
    assert(responses[0].body == "first");
    assert(header_value(responses[0], "Connection") == "close");
  }

  static void test_missing_route_closes_connection_and_stops_later_requests()
  {
    Router router;

    router.add_route(
        "GET",
        "/after",
        text_handler("after"));

    const std::string raw =
        get_request("/missing", "keep-alive") +
        get_request("/after", "keep-alive");

    const auto responses = parse_all_responses(
        run_session(router, raw));

    assert(responses.size() == 1);

    const auto &res = responses[0];

    assert(res.status == vix::http::NOT_FOUND);
    assert(header_value(res, "Connection") == "close");

    const auto body = vix::json::loads(res.body);

    assert(body["error"].get<std::string>() == "Route not found");
    assert(body["path"].get<std::string>() == "/missing");
  }

  static void test_head_keep_alive_has_no_body_but_keeps_original_length()
  {
    Router router;

    router.add_route(
        "GET",
        "/head",
        text_handler("head-body"));

    const RawResponse res = parse_header_only_response(
        run_session(router, head_request("/head", "keep-alive")));

    assert(res.status == vix::http::OK);
    assert(res.body.empty());
    assert(header_value(res, "Content-Length") == "9");
    assert(header_value(res, "Connection") == "keep-alive");
  }

  static void test_keep_alive_with_small_transport_chunks()
  {
    Router router;

    router.add_route(
        "GET",
        "/one",
        text_handler("one"));

    router.add_route(
        "GET",
        "/two",
        text_handler("two"));

    const std::string raw =
        get_request("/one", "keep-alive") +
        get_request("/two", "close");

    const auto responses = parse_all_responses(
        run_session(router, raw, 7));

    assert(responses.size() == 2);

    assert(responses[0].status == vix::http::OK);
    assert(responses[0].body == "one");
    assert(header_value(responses[0], "Connection") == "keep-alive");

    assert(responses[1].status == vix::http::OK);
    assert(responses[1].body == "two");
    assert(header_value(responses[1], "Connection") == "close");
  }

} // namespace

int main()
{
  test_http_11_defaults_to_keep_alive();
  test_explicit_keep_alive_is_preserved();
  test_explicit_connection_close_is_preserved();
  test_connection_close_is_case_insensitive_for_shutdown();

  test_multiple_keep_alive_requests_on_same_connection();
  test_connection_close_stops_processing_later_pipelined_requests();
  test_missing_route_closes_connection_and_stops_later_requests();

  test_head_keep_alive_has_no_body_but_keeps_original_length();
  test_keep_alive_with_small_transport_chunks();

  return 0;
}
