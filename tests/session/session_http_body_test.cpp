/**
 *
 * @file session_http_body_test.cpp
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

  class EchoBodyHandler final : public vix::http::IRequestHandler
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
              {"body", req.body()},
              {"body_size", req.body().size()},
              {"content_type", req.header("Content-Type")},
              {"content_length", req.header("Content-Length")},
          },
          vix::http::OK);

      co_return;
    }
  };

  class ParseJsonBodyHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) override
    {
      const auto body = req.json();

      Response::json_response(
          res,
          vix::json::Json{
              {"ok", true},
              {"name", body.value("name", "")},
              {"count", body.value("count", 0)},
              {"active", body.value("active", false)},
          },
          vix::http::CREATED);

      co_return;
    }
  };

  static std::shared_ptr<vix::http::IRequestHandler> echo_body_handler()
  {
    return std::make_shared<EchoBodyHandler>();
  }

  static std::shared_ptr<vix::http::IRequestHandler> parse_json_body_handler()
  {
    return std::make_shared<ParseJsonBodyHandler>();
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

  static std::string build_raw_request(
      std::string method,
      std::string target,
      std::string body,
      std::string content_type = "application/json",
      std::string connection = "close")
  {
    std::string raw;

    raw += method;
    raw += " ";
    raw += target;
    raw += " HTTP/1.1\r\n";
    raw += "Host: localhost\r\n";

    if (!content_type.empty())
    {
      raw += "Content-Type: ";
      raw += content_type;
      raw += "\r\n";
    }

    raw += "Content-Length: ";
    raw += std::to_string(body.size());
    raw += "\r\n";

    if (!connection.empty())
    {
      raw += "Connection: ";
      raw += connection;
      raw += "\r\n";
    }

    raw += "\r\n";
    raw += body;

    return raw;
  }

  static std::string run_session_once(
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

  static void assert_common_response_headers(const RawResponse &res)
  {
    assert(header_value(res, "Server") == "Vix.cpp");
    assert(!header_value(res, "Date").empty());
    assert(!header_value(res, "Content-Length").empty());
    assert(!header_value(res, "Connection").empty());
  }

  static void test_post_json_body_is_read_and_exposed()
  {
    Router router;

    router.add_route(
        "POST",
        "/echo",
        echo_body_handler());

    const std::string body = R"({"name":"Vix","count":3})";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("POST", "/echo", body)));

    assert(res.status == vix::http::OK);
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");
    assert(header_value(res, "Connection") == "close");
    assert_common_response_headers(res);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "POST");
    assert(parsed["target"].get<std::string>() == "/echo");
    assert(parsed["path"].get<std::string>() == "/echo");
    assert(parsed["query"].get<std::string>().empty());
    assert(parsed["body"].get<std::string>() == body);
    assert(parsed["body_size"].get<std::size_t>() == body.size());
    assert(parsed["content_type"].get<std::string>() == "application/json");
    assert(parsed["content_length"].get<std::string>() == std::to_string(body.size()));
  }

  static void test_post_json_body_can_be_parsed_by_request_json()
  {
    Router router;

    router.add_route(
        "POST",
        "/users",
        parse_json_body_handler());

    const std::string body = R"({"name":"Gaspard","count":7,"active":true})";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("POST", "/users", body)));

    assert(res.status == vix::http::CREATED);
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");
    assert_common_response_headers(res);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["ok"].get<bool>() == true);
    assert(parsed["name"].get<std::string>() == "Gaspard");
    assert(parsed["count"].get<int>() == 7);
    assert(parsed["active"].get<bool>() == true);
  }

  static void test_put_text_body_is_read()
  {
    Router router;

    router.add_route(
        "PUT",
        "/items/7",
        echo_body_handler());

    const std::string body = "updated item body";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request(
                "PUT",
                "/items/7",
                body,
                "text/plain")));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "PUT");
    assert(parsed["path"].get<std::string>() == "/items/7");
    assert(parsed["body"].get<std::string>() == body);
    assert(parsed["body_size"].get<std::size_t>() == body.size());
    assert(parsed["content_type"].get<std::string>() == "text/plain");
  }

  static void test_patch_json_body_is_read()
  {
    Router router;

    router.add_route(
        "PATCH",
        "/users/42",
        echo_body_handler());

    const std::string body = R"({"name":"Updated"})";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("PATCH", "/users/42", body)));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "PATCH");
    assert(parsed["target"].get<std::string>() == "/users/42");
    assert(parsed["body"].get<std::string>() == body);
  }

  static void test_delete_body_is_read()
  {
    Router router;

    router.add_route(
        "DELETE",
        "/items/7",
        echo_body_handler());

    const std::string body = R"({"reason":"cleanup"})";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("DELETE", "/items/7", body)));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "DELETE");
    assert(parsed["body"].get<std::string>() == body);
    assert(parsed["body_size"].get<std::size_t>() == body.size());
  }

  static void test_post_body_with_query_string()
  {
    Router router;

    router.add_route(
        "POST",
        "/submit",
        echo_body_handler());

    const std::string body = R"({"ok":true})";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("POST", "/submit?debug=1", body)));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "POST");
    assert(parsed["target"].get<std::string>() == "/submit?debug=1");
    assert(parsed["path"].get<std::string>() == "/submit");
    assert(parsed["query"].get<std::string>() == "debug=1");
    assert(parsed["body"].get<std::string>() == body);
  }

  static void test_post_empty_body_with_content_length_zero()
  {
    Router router;

    router.add_route(
        "POST",
        "/empty",
        echo_body_handler());

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("POST", "/empty", "")));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "POST");
    assert(parsed["body"].get<std::string>().empty());
    assert(parsed["body_size"].get<std::size_t>() == 0);
    assert(parsed["content_length"].get<std::string>() == "0");
  }

  static void test_body_can_be_read_after_header_block()
  {
    Router router;

    router.add_route(
        "POST",
        "/chunked-by-transport",
        echo_body_handler());

    const std::string body = R"({"transport":"small-chunks"})";

    const std::string raw_request = build_raw_request(
        "POST",
        "/chunked-by-transport",
        body);

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            raw_request,
            8));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["body"].get<std::string>() == body);
    assert(parsed["body_size"].get<std::size_t>() == body.size());
  }

  static void test_get_with_content_length_does_not_read_body()
  {
    Router router;

    router.add_route(
        "GET",
        "/ignored-body",
        echo_body_handler());

    const std::string body = R"({"should":"be ignored"})";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request("GET", "/ignored-body", body)));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "GET");
    assert(parsed["target"].get<std::string>() == "/ignored-body");
    assert(parsed["body"].get<std::string>().empty());
    assert(parsed["body_size"].get<std::size_t>() == 0);
    assert(parsed["content_length"].get<std::string>() == std::to_string(body.size()));
  }

  static void test_post_without_content_type_still_reads_body()
  {
    Router router;

    router.add_route(
        "POST",
        "/no-content-type",
        echo_body_handler());

    const std::string body = "plain body";

    const RawResponse res = parse_raw_response(
        run_session_once(
            router,
            build_raw_request(
                "POST",
                "/no-content-type",
                body,
                "")));

    assert(res.status == vix::http::OK);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["body"].get<std::string>() == body);
    assert(parsed["content_type"].get<std::string>().empty());
    assert(parsed["content_length"].get<std::string>() == std::to_string(body.size()));
  }

} // namespace

int main()
{
  test_post_json_body_is_read_and_exposed();
  test_post_json_body_can_be_parsed_by_request_json();

  test_put_text_body_is_read();
  test_patch_json_body_is_read();
  test_delete_body_is_read();

  test_post_body_with_query_string();
  test_post_empty_body_with_content_length_zero();
  test_body_can_be_read_after_header_block();

  test_get_with_content_length_does_not_read_body();
  test_post_without_content_type_still_reads_body();

  return 0;
}
