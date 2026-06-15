/**
 *
 * @file session_waf_test.cpp
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
#include <cstdlib>

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

  class EchoHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) override
    {
      Response::json_response(
          res,
          vix::json::Json{
              {"ok", true},
              {"method", req.method()},
              {"target", req.target()},
              {"path", req.path()},
              {"body", req.body()},
              {"body_size", req.body().size()},
          },
          vix::http::OK);

      co_return;
    }
  };

  static std::shared_ptr<vix::http::IRequestHandler> echo_handler()
  {
    return std::make_shared<EchoHandler>();
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
  }

  static void set_env_var(const char *name, const std::string &value)
  {
#if defined(_WIN32)
    const std::string assignment = std::string{name} + "=" + value;
    const int rc = _putenv(assignment.c_str());
    assert(rc == 0);
#else
    const int rc = setenv(name, value.c_str(), 1);
    assert(rc == 0);
#endif
  }

  static vix::async::core::task<void> run_session_task(
      vix::async::core::io_context &ctx,
      std::shared_ptr<Session> session)
  {
    co_await session->run();

    ctx.stop();

    co_return;
  }

  static Config make_config(
      std::string mode,
      int max_target_len,
      int max_body_bytes)
  {
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("SERVER_PORT", "8080");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_BENCH_MODE", "false");
    set_env_var("LOGGING_ASYNC", "false");

    set_env_var("WAF_MODE", mode);
    set_env_var("WAF_MAX_TARGET_LEN", std::to_string(max_target_len));
    set_env_var("WAF_MAX_BODY_BYTES", std::to_string(max_body_bytes));

    Config config{};

    assert(config.getWafMode() == mode);
    assert(config.getWafMaxTargetLen() == max_target_len);
    assert(config.getWafMaxBodyBytes() == max_body_bytes);

    return config;
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

  static std::string build_get_request(
      std::string target,
      std::string connection = "close")
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

  static std::string build_body_request(
      std::string method,
      std::string target,
      std::string body,
      std::string content_type = "text/plain",
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
      const Config &config,
      const std::string &raw_request)
  {
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
      const Config &config,
      const std::string &raw_request)
  {
    return parse_raw_response(
        run_session_once(router, config, raw_request));
  }

  static void assert_allowed_response(const RawResponse &res)
  {
    assert(res.status == vix::http::OK);
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");

    const auto body = vix::json::loads(res.body);

    assert(body["ok"].get<bool>() == true);
  }

  static void assert_blocked_response(const RawResponse &res)
  {
    assert(res.status == vix::http::BAD_REQUEST);
    assert(res.reason == "Bad Request");
    assert(header_value(res, "Content-Type") == "application/json; charset=utf-8");
    assert(header_value(res, "Connection") == "close");

    const auto body = vix::json::loads(res.body);

    assert(body["message"].get<std::string>() == "Request blocked (security)");
  }

  static void add_echo_routes(Router &router)
  {
    router.add_route("GET", "/safe", echo_handler());
    router.add_route("GET", "/search", echo_handler());
    router.add_route("GET", "/script", echo_handler());

    router.add_route("POST", "/submit", echo_handler());
    router.add_route("PUT", "/submit", echo_handler());
    router.add_route("PATCH", "/submit", echo_handler());
    router.add_route("DELETE", "/submit", echo_handler());
  }

  static void test_basic_waf_allows_safe_get_request()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_get_request("/safe"));

    assert_allowed_response(res);

    const auto body = vix::json::loads(res.body);

    assert(body["method"].get<std::string>() == "GET");
    assert(body["target"].get<std::string>() == "/safe");
  }

  static void test_basic_waf_allows_safe_post_body_without_triggers()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const std::string body = "normal body without dangerous tokens";

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request("POST", "/submit", body));

    assert_allowed_response(res);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "POST");
    assert(parsed["body"].get<std::string>() == body);
  }

  static void test_basic_waf_blocks_xss_in_target()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_get_request("/script?<script>alert(1)</script>"));

    assert_blocked_response(res);
  }

  static void test_basic_waf_blocks_xss_in_mutating_body()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request(
            "POST",
            "/submit",
            "<script>alert(1)</script>"));

    assert_blocked_response(res);
  }

  static void test_basic_waf_blocks_sql_keyword_in_mutating_body()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request(
            "POST",
            "/submit",
            "name=test UNION SELECT password FROM users"));

    assert_blocked_response(res);
  }

  static void test_basic_waf_blocks_sql_keyword_case_insensitively()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request(
            "PATCH",
            "/submit",
            "please DrOp table users"));

    assert_blocked_response(res);
  }

  static void test_basic_waf_does_not_scan_safe_get_body_because_get_body_is_ignored()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request(
            "GET",
            "/safe",
            "<script>alert(1)</script>"));

    assert_allowed_response(res);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["method"].get<std::string>() == "GET");
    assert(parsed["body"].get<std::string>().empty());
  }

  static void test_basic_waf_blocks_target_longer_than_limit()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 12, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_get_request("/safe/too-long-target"));

    assert_blocked_response(res);
  }

  static void test_basic_waf_blocks_mutating_body_larger_than_limit()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("basic", 4096, 8);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request(
            "POST",
            "/submit",
            "123456789"));

    assert_blocked_response(res);
  }

  static void test_waf_off_allows_xss_target()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("off", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_get_request("/script?<script>alert(1)</script>"));

    assert_allowed_response(res);

    const auto body = vix::json::loads(res.body);

    assert(body["target"].get<std::string>() == "/script?<script>alert(1)</script>");
  }

  static void test_waf_off_allows_sql_body()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("off", 4096, 1024);

    const std::string body = "UNION SELECT password FROM users";

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request("POST", "/submit", body));

    assert_allowed_response(res);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["body"].get<std::string>() == body);
  }

  static void test_strict_waf_allows_safe_body()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("strict", 4096, 1024);

    const std::string body = "normal strict body";

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request("POST", "/submit", body));

    assert_allowed_response(res);

    const auto parsed = vix::json::loads(res.body);

    assert(parsed["body"].get<std::string>() == body);
  }

  static void test_strict_waf_blocks_sql_body()
  {
    Router router;
    add_echo_routes(router);

    const Config config = make_config("strict", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_body_request(
            "DELETE",
            "/submit",
            "DELETE FROM sessions"));

    assert_blocked_response(res);
  }

  static void test_waf_block_happens_before_router_404()
  {
    Router router;

    const Config config = make_config("basic", 4096, 1024);

    const RawResponse res = run_and_parse(
        router,
        config,
        build_get_request("/missing?<script>alert(1)</script>"));

    assert_blocked_response(res);
  }

} // namespace

int main()
{
  test_basic_waf_allows_safe_get_request();
  test_basic_waf_allows_safe_post_body_without_triggers();

  test_basic_waf_blocks_xss_in_target();
  test_basic_waf_blocks_xss_in_mutating_body();
  test_basic_waf_blocks_sql_keyword_in_mutating_body();
  test_basic_waf_blocks_sql_keyword_case_insensitively();

  test_basic_waf_does_not_scan_safe_get_body_because_get_body_is_ignored();

  test_basic_waf_blocks_target_longer_than_limit();
  test_basic_waf_blocks_mutating_body_larger_than_limit();

  test_waf_off_allows_xss_target();
  test_waf_off_allows_sql_body();

  test_strict_waf_allows_safe_body();
  test_strict_waf_blocks_sql_body();

  test_waf_block_happens_before_router_404();

  return 0;
}
