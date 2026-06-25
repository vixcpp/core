/**
 *
 * @file Session.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/session/Session.hpp>
#include <vix/session/PlainTransport.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <vix/async/core/spawn.hpp>
#include <vix/http/Response.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/NetworkError.hpp>

namespace vix::session
{
  using Logger = vix::utils::Logger;
  using vix::async::core::spawn_detached;

  namespace
  {
    inline Logger &log()
    {
      return Logger::getInstance();
    }

    inline bool starts_with_icase(std::string_view s, std::string_view prefix)
    {
      if (prefix.size() > s.size())
      {
        return false;
      }

      for (std::size_t i = 0; i < prefix.size(); ++i)
      {
        const unsigned char a = static_cast<unsigned char>(s[i]);
        const unsigned char b = static_cast<unsigned char>(prefix[i]);

        if (static_cast<char>(std::tolower(a)) !=
            static_cast<char>(std::tolower(b)))
        {
          return false;
        }
      }

      return true;
    }

    inline bool equals_icase(std::string_view a, std::string_view b)
    {
      if (a.size() != b.size())
      {
        return false;
      }

      for (std::size_t i = 0; i < a.size(); ++i)
      {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);

        if (static_cast<char>(std::tolower(ca)) !=
            static_cast<char>(std::tolower(cb)))
        {
          return false;
        }
      }

      return true;
    }

    task<bool> write_all(
        Transport &transport,
        std::string_view data,
        vix::async::core::cancel_token token)
    {
      std::size_t written = 0;

      while (written < data.size())
      {
        const std::byte *ptr =
            reinterpret_cast<const std::byte *>(data.data() + written);

        const std::size_t n = co_await transport.async_write(
            std::span<const std::byte>(ptr, data.size() - written),
            token);

        if (n == 0)
        {
          co_return false;
        }

        written += n;
      }

      co_return true;
    }

    inline bool is_close_connection(std::string_view value)
    {
      return equals_icase(value, "close");
    }

    inline const std::string &cached_date_header()
    {
      using clock = std::chrono::system_clock;

      static std::string cached = vix::http::Response::http_date_now();
      static auto last_tick = clock::now();
      static std::atomic_flag lock = ATOMIC_FLAG_INIT;

      const auto now = clock::now();
      if (now - last_tick < std::chrono::seconds(1))
      {
        return cached;
      }

      while (lock.test_and_set(std::memory_order_acquire))
      {
      }

      if (clock::now() - last_tick >= std::chrono::seconds(1))
      {
        cached = vix::http::Response::http_date_now();
        last_tick = clock::now();
      }

      lock.clear(std::memory_order_release);
      return cached;
    }

    inline std::size_t find_header_terminator(const std::string &s)
    {
      const auto pos = s.find("\r\n\r\n");
      if (pos == std::string::npos)
      {
        return std::string::npos;
      }

      return pos + 4;
    }

#ifdef VIX_BENCH_MODE
    inline bool raw_header_requests_close(std::string_view raw_header)
    {
      std::size_t line_start = 0;

      while (line_start < raw_header.size())
      {
        std::size_t line_end = raw_header.find("\r\n", line_start);
        if (line_end == std::string_view::npos)
        {
          line_end = raw_header.size();
        }

        const std::string_view line =
            raw_header.substr(line_start, line_end - line_start);

        if (line.empty())
        {
          break;
        }

        constexpr std::string_view prefix = "Connection:";
        if (line.size() >= prefix.size() && starts_with_icase(line, prefix))
        {
          std::size_t value_start = prefix.size();

          while (value_start < line.size() &&
                 std::isspace(static_cast<unsigned char>(line[value_start])))
          {
            ++value_start;
          }

          const std::string_view value = line.substr(value_start);
          return equals_icase(value, "close");
        }

        if (line_end == raw_header.size())
        {
          break;
        }

        line_start = line_end + 2;
      }

      return false;
    }

    inline std::string_view strip_query_view(std::string_view target)
    {
      const auto q = target.find('?');
      if (q == std::string_view::npos)
      {
        return target;
      }

      return target.substr(0, q);
    }

    inline bool raw_header_is_bench_get(std::string_view raw_header)
    {
      const std::size_t line_end = raw_header.find("\r\n");
      const std::string_view request_line =
          line_end == std::string_view::npos
              ? raw_header
              : raw_header.substr(0, line_end);

      const std::size_t sp1 = request_line.find(' ');
      if (sp1 == std::string_view::npos)
      {
        return false;
      }

      const std::size_t sp2 = request_line.find(' ', sp1 + 1);
      if (sp2 == std::string_view::npos)
      {
        return false;
      }

      const std::string_view method = request_line.substr(0, sp1);
      const std::string_view target =
          request_line.substr(sp1 + 1, sp2 - sp1 - 1);

      return equals_icase(method, "GET") && strip_query_view(target) == "/bench";
    }
#endif

    inline std::vector<std::string_view> split_lines(std::string_view block)
    {
      std::vector<std::string_view> out;
      std::size_t start = 0;

      while (start < block.size())
      {
        std::size_t end = block.find("\r\n", start);

        if (end == std::string_view::npos)
        {
          out.emplace_back(block.substr(start));
          break;
        }

        out.emplace_back(block.substr(start, end - start));
        start = end + 2;
      }

      return out;
    }

  } // namespace

  const std::regex Session::XSS_PATTERN(
      R"(<script.*?>.*?</script>)",
      std::regex::icase);

  const std::regex Session::SQL_PATTERN(
      R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))",
      std::regex::icase);

  Session::Session(
      std::unique_ptr<tcp_stream> stream,
      vix::router::Router &router,
      const vix::config::Config &config,
      std::shared_ptr<vix::executor::RuntimeExecutor> executor)
      : Session(
            std::make_unique<PlainTransport>(std::move(stream)),
            router,
            config,
            std::move(executor))
  {
  }

  Session::Session(
      std::unique_ptr<Transport> transport,
      vix::router::Router &router,
      const vix::config::Config &config,
      std::shared_ptr<vix::executor::RuntimeExecutor> executor)
      : transport_(std::move(transport)),
        router_(router),
        config_(config),
        executor_(std::move(executor)),
        read_buffer_(),
        io_context_(nullptr),
        timer_cancel_()
  {
    if (!transport_)
    {
      throw std::invalid_argument("Session requires a valid transport");
    }

    if (!executor_)
    {
      throw std::invalid_argument("Session requires a valid executor");
    }

    read_buffer_.reserve(8192);
  }

  task<void> Session::run()
  {
    try
    {
#ifdef VIX_BENCH_MODE
      while (transport_ && transport_->is_open())
      {
        if (!co_await handle_bench_request_fast())
        {
          break;
        }
      }
#else
      while (transport_ && transport_->is_open())
      {
        auto maybe_req = co_await read_request();

        if (!maybe_req.has_value())
        {
          break;
        }

        co_await dispatch_request(std::move(*maybe_req));

        if (!transport_ || !transport_->is_open())
        {
          break;
        }
      }
#endif
    }
    catch (const std::system_error &e)
    {
      if (!is_normal_disconnect(e))
      {
        log().log(
            Logger::Level::Error,
            "[session] fatal session error: {}",
            e.what());
      }
    }
    catch (const std::exception &e)
    {
      if (!vix::utils::is_normal_network_disconnect_message(e.what()))
      {
        log().log(
            Logger::Level::Error,
            "[session] fatal session error: {}",
            e.what());
      }
    }

    co_await close_stream_gracefully();
    co_return;
  }

  task<bool> Session::handle_bench_request_fast()
  {
#ifdef VIX_BENCH_MODE
    if (!transport_ || !transport_->is_open())
    {
      co_return false;
    }

    constexpr std::size_t MAX_HEADER_BYTES = 64 * 1024;

    while (true)
    {
      const auto pos = find_header_terminator(read_buffer_);
      if (pos != std::string::npos)
      {
        if (pos > MAX_HEADER_BYTES)
        {
          co_return false;
        }

        const std::string_view raw_header{read_buffer_.data(), pos};

        static constexpr std::string_view kBenchResponse =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 2\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "OK";

        static constexpr std::string_view kNotFound =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";

        const bool is_bench = raw_header_is_bench_get(raw_header);
        const std::string_view wire = is_bench ? kBenchResponse : kNotFound;

        read_buffer_.erase(0, pos);

        const bool ok = co_await write_all(*transport_, wire, timer_cancel_.token());
        if (!ok || !is_bench)
        {
          co_await close_stream_gracefully();
          co_return false;
        }

        co_return true;
      }

      if (read_buffer_.size() > MAX_HEADER_BYTES)
      {
        co_return false;
      }

      std::array<std::byte, 8192> chunk{};
      const std::size_t n = co_await transport_->async_read(
          std::span<std::byte>(chunk.data(), chunk.size()),
          timer_cancel_.token());

      if (n == 0)
      {
        co_return false;
      }

      read_buffer_.append(
          reinterpret_cast<const char *>(chunk.data()),
          n);
    }
#else
    co_return false;
#endif
  }

  void Session::start_timer()
  {
#ifdef VIX_BENCH_MODE
    return;
#else
    if (config_.isBenchMode())
    {
      return;
    }

    timer_cancel_ = cancel_source{};

    if (!io_context_)
    {
      return;
    }

    const auto timeout = std::chrono::seconds(config_.getSessionTimeoutSec());
    auto ct = timer_cancel_.token();
    auto weak_self = weak_from_this();

    spawn_detached(
        *io_context_,
        [timeout, ct, weak_self]() -> task<void>
        {
          try
          {
            auto self = weak_self.lock();

            if (!self || !self->io_context_)
            {
              co_return;
            }

            co_await self->io_context_->timers().sleep_for(timeout, ct);

            self = weak_self.lock();

            if (!self)
            {
              co_return;
            }

            co_await self->close_stream_gracefully();
          }
          catch (...)
          {
          }

          co_return;
        }());
#endif
  }

  void Session::cancel_timer()
  {
#ifdef VIX_BENCH_MODE
    return;
#else
    if (config_.isBenchMode())
    {
      return;
    }

    timer_cancel_.request_cancel();
#endif
  }

  bool Session::is_normal_disconnect(const std::system_error &e) noexcept
  {
    return vix::utils::is_normal_network_disconnect(e);
  }

  task<std::optional<vix::http::Request>> Session::read_request()
  {
    if (!transport_ || !transport_->is_open())
    {
      co_return std::nullopt;
    }

#ifndef VIX_BENCH_MODE
    if (!config_.isBenchMode())
    {
      cancel_timer();
      start_timer();
    }
#endif

    bool malformed_request = false;

    try
    {
      const std::string raw_header = co_await read_header_block();

      if (raw_header.empty())
      {
#ifndef VIX_BENCH_MODE
        if (!config_.isBenchMode())
        {
          cancel_timer();
        }
#endif
        co_return std::nullopt;
      }

      ParsedRequestHead head = parse_request_head(raw_header);
      std::string body = co_await read_request_body(head);

#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      auto req = make_request(std::move(head), std::move(body));

      if (!config_.isBenchMode() && !waf_check_request(req))
      {
        co_await send_error(vix::http::BAD_REQUEST, "Request blocked (security)");
        co_return std::nullopt;
      }

      if (req.body().size() > MAX_REQUEST_BODY_SIZE)
      {
        co_await send_error(vix::http::PAYLOAD_TOO_LARGE, "Request too large");
        co_return std::nullopt;
      }

      co_return req;
    }
    catch (const std::system_error &e)
    {
#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      if (is_normal_disconnect(e))
      {
        log().log(
            Logger::Level::Debug,
            "[session] client disconnected: {}",
            e.what());

        co_return std::nullopt;
      }

      log().log(
          Logger::Level::Error,
          "[session] read/system error: {}",
          e.what());

      co_return std::nullopt;
    }
    catch (const std::exception &e)
    {
#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      log().log(
          Logger::Level::Error,
          "[session] request parse error: {}",
          e.what());

      malformed_request = true;
    }

    if (malformed_request)
    {
      co_await send_error(vix::http::BAD_REQUEST, "Malformed HTTP request");
    }

    co_return std::nullopt;
  }

  task<void> Session::dispatch_request(vix::http::Request req)
  {
#ifdef VIX_BENCH_MODE
    {
      const std::string_view method{req.method()};
      const std::string_view target = strip_query_view(req.target());

      if (equals_icase(method, "GET") && target == "/bench")
      {
        static constexpr std::string_view kBenchResponse =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: keep-alive\r\n"
            "Server: Vix.cpp\r\n"
            "\r\n"
            "OK";

        static constexpr std::string_view kBenchResponseClose =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n"
            "Server: Vix.cpp\r\n"
            "\r\n"
            "OK";

        const std::string conn = req.header("Connection");
        const bool close_requested = !conn.empty() && is_close_connection(conn);

        const std::string_view wire =
            close_requested ? kBenchResponseClose : kBenchResponse;

        const bool ok = co_await write_all(*transport_, wire, timer_cancel_.token());

        if (!ok || close_requested)
        {
          co_await close_stream_gracefully();
        }

        co_return;
      }
    }
#endif

    vix::http::Response res;

    try
    {
      (void)executor_;
      (void)co_await router_.handle_request(req, res);
    }
    catch (const std::exception &ex)
    {
      log().log(
          Logger::Level::Error,
          "[router] exception: {}",
          ex.what());

      vix::http::Response::error_response(
          res,
          vix::http::INTERNAL_ERROR,
          "Internal server error");

      res.set_should_close(true);
      res.set_header("Connection", "close");
    }

    const std::string conn = req.header("Connection");

    if (!res.has_header("Connection"))
    {
      if (!conn.empty())
      {
        res.set_header("Connection", conn);
        res.set_should_close(is_close_connection(conn));
      }
      else
      {
        res.set_header("Connection", "keep-alive");
        res.set_should_close(false);
      }
    }

    co_await send_response(std::move(res));
    co_return;
  }

  task<void> Session::send_response(vix::http::Response res)
  {
    if (!transport_ || !transport_->is_open())
    {
      co_return;
    }

#ifndef VIX_BENCH_MODE
    if (!config_.isBenchMode())
    {
      cancel_timer();
      start_timer();
    }
#endif

    bool must_close = false;

    try
    {
      if (!res.has_header("Server"))
      {
        res.set_header("Server", "Vix.cpp");
      }

      if (!res.has_header("Date"))
      {
        res.set_header("Date", cached_date_header());
      }

      if (!res.has_header("Content-Length"))
      {
        res.set_header("Content-Length", std::to_string(res.body().size()));
      }

      if (!res.has_header("Connection"))
      {
        res.set_header("Connection", res.should_close() ? "close" : "keep-alive");
      }

      std::string header_block;
      header_block.reserve(256 + res.headers().size() * 32);

      header_block += res.version();
      header_block += ' ';
      header_block += std::to_string(res.status());
      header_block += ' ';

      if (!res.reason().empty())
      {
        header_block += res.reason();
      }
      else
      {
        header_block += vix::http::reason_phrase(res.status());
      }

      header_block += "\r\n";

      for (const auto &[name, value] : res.headers())
      {
        header_block += name;
        header_block += ": ";
        header_block += value;
        header_block += "\r\n";
      }

      header_block += "\r\n";

      const std::string &body = res.body();
      const auto token = timer_cancel_.token();

      constexpr std::size_t SMALL_RESPONSE_THRESHOLD = 1024;

      if (body.size() <= SMALL_RESPONSE_THRESHOLD)
      {
        std::string wire;
        wire.reserve(header_block.size() + body.size());
        wire += header_block;
        wire += body;

        const bool ok = co_await write_all(*transport_, wire, token);

        if (!ok)
        {
          must_close = true;
        }
      }
      else
      {
        const bool header_ok = co_await write_all(*transport_, header_block, token);

        bool body_ok = false;

        if (header_ok)
        {
          body_ok = co_await write_all(*transport_, body, token);
        }

        if (!header_ok || !body_ok)
        {
          must_close = true;
        }
      }

#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      if (res.should_close())
      {
        must_close = true;
      }
    }
    catch (const std::system_error &e)
    {
#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      if (is_normal_disconnect(e))
      {
        log().log(
            Logger::Level::Debug,
            "[session] client disconnected during response write: {}",
            e.what());

        must_close = true;
      }
      else
      {
        log().log(
            Logger::Level::Error,
            "[session] write/system error: {}",
            e.what());

        must_close = true;
      }
    }
    catch (const std::exception &e)
    {
#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      if (vix::utils::is_normal_network_disconnect_message(e.what()))
      {
        log().log(
            Logger::Level::Debug,
            "[session] client disconnected during response write: {}",
            e.what());

        must_close = true;
      }
      else
      {
        log().log(
            Logger::Level::Error,
            "[session] write error: {}",
            e.what());

        must_close = true;
      }
    }

    if (must_close)
    {
      co_await close_stream_gracefully();
    }

    co_return;
  }

  task<void> Session::send_error(int status, const std::string &msg)
  {
    vix::http::Response res;
    vix::http::Response::error_response(res, status, msg);
    res.set_should_close(true);
    res.set_header("Connection", "close");

    co_await send_response(std::move(res));
    co_return;
  }

  task<void> Session::close_stream_gracefully()
  {
#ifndef VIX_BENCH_MODE
    if (!config_.isBenchMode())
    {
      cancel_timer();
    }
#endif

    if (!transport_)
    {
      co_return;
    }

    try
    {
      if (transport_->is_open())
      {
        transport_->close();
      }
    }
    catch (...)
    {
    }

    co_return;
  }

  bool Session::waf_check_request(const vix::http::Request &req)
  {
#ifdef VIX_BENCH_MODE
    (void)req;
    return true;
#else
    const std::string &mode = config_.getWafMode();

    if (mode == "off")
    {
      return true;
    }

    const std::size_t maxTargetLen =
        static_cast<std::size_t>(config_.getWafMaxTargetLen());

    const std::size_t maxBodyBytes =
        static_cast<std::size_t>(config_.getWafMaxBodyBytes());

    if (req.target().size() > maxTargetLen)
    {
      return false;
    }

    for (char c : req.target())
    {
      if (c == '\0' || c == '\r' || c == '\n')
      {
        return false;
      }
    }

    std::string_view target{req.target()};

    const bool suspicious_url =
        target.find('<') != std::string_view::npos ||
        vix::utils::contains_token_icase(target, "script") ||
        vix::utils::contains_token_icase(target, "union") ||
        vix::utils::contains_token_icase(target, "select") ||
        vix::utils::contains_token_icase(target, "drop");

    if (suspicious_url)
    {
      try
      {
        const std::string t(target);

        if (std::regex_search(t, XSS_PATTERN))
        {
          return false;
        }
      }
      catch (...)
      {
        return false;
      }
    }

    const std::string method = to_lower(req.method());

    const bool mutating =
        (method == "post" ||
         method == "put" ||
         method == "patch" ||
         method == "delete");

    if (!mutating)
    {
      return true;
    }

    const std::string &body = req.body();

    if (body.empty())
    {
      return true;
    }

    if (body.size() > maxBodyBytes)
    {
      return false;
    }

    const bool cheap_trigger =
        body.find('<') != std::string::npos ||
        vix::utils::contains_token_icase(body, "union") ||
        vix::utils::contains_token_icase(body, "select") ||
        vix::utils::contains_token_icase(body, "drop") ||
        vix::utils::contains_token_icase(body, "insert") ||
        vix::utils::contains_token_icase(body, "delete") ||
        vix::utils::contains_token_icase(body, "update");

    if (mode == "basic" && !cheap_trigger)
    {
      return true;
    }

    try
    {
      if (std::regex_search(body, SQL_PATTERN))
      {
        return false;
      }

      if (std::regex_search(body, XSS_PATTERN))
      {
        return false;
      }
    }
    catch (...)
    {
      return false;
    }

    return true;
#endif
  }

  task<std::string> Session::read_header_block()
  {
    constexpr std::size_t MAX_HEADER_BYTES = 64 * 1024;

    while (true)
    {
      const auto pos = find_header_terminator(read_buffer_);

      if (pos != std::string::npos)
      {
        if (pos > MAX_HEADER_BYTES)
        {
          throw std::runtime_error("HTTP header too large");
        }

        std::string header = read_buffer_.substr(0, pos);
        read_buffer_.erase(0, pos);
        co_return header;
      }

      if (read_buffer_.size() > MAX_HEADER_BYTES)
      {
        throw std::runtime_error("HTTP header too large");
      }

      std::array<std::byte, 8192> chunk{};

      const std::size_t n = co_await transport_->async_read(
          std::span<std::byte>(chunk.data(), chunk.size()),
          timer_cancel_.token());

      if (n == 0)
      {
        co_return std::string{};
      }

      read_buffer_.append(
          reinterpret_cast<const char *>(chunk.data()),
          n);
    }
  }

  ParsedRequestHead Session::parse_request_head(const std::string &raw_header) const
  {
    ParsedRequestHead head{};

    std::string_view block{raw_header};

    if (block.size() >= 4 && block.substr(block.size() - 4) == "\r\n\r\n")
    {
      block.remove_suffix(4);
    }

    const auto lines = split_lines(block);

    if (lines.empty())
    {
      throw std::runtime_error("empty request head");
    }

    {
      const std::string_view request_line = lines.front();

      const std::size_t sp1 = request_line.find(' ');

      if (sp1 == std::string_view::npos)
      {
        throw std::runtime_error("invalid request line");
      }

      const std::size_t sp2 = request_line.find(' ', sp1 + 1);

      if (sp2 == std::string_view::npos)
      {
        throw std::runtime_error("invalid request line");
      }

      head.method = std::string(request_line.substr(0, sp1));
      head.target = std::string(request_line.substr(sp1 + 1, sp2 - sp1 - 1));
      head.version = std::string(request_line.substr(sp2 + 1));

      if (head.method.empty() || head.target.empty() || head.version.empty())
      {
        throw std::runtime_error("invalid request line fields");
      }

      if (!starts_with_icase(head.version, "HTTP/"))
      {
        throw std::runtime_error("unsupported HTTP version");
      }
    }

    head.headers.reserve(lines.size() > 0 ? lines.size() - 1 : 0);

    for (std::size_t i = 1; i < lines.size(); ++i)
    {
      const std::string_view line = lines[i];

      if (line.empty())
      {
        continue;
      }

      const std::size_t colon = line.find(':');

      if (colon == std::string_view::npos)
      {
        throw std::runtime_error("malformed header");
      }

      std::string key = trim(std::string(line.substr(0, colon)));
      std::string value = trim(std::string(line.substr(colon + 1)));

      if (key.empty())
      {
        throw std::runtime_error("empty header name");
      }

      head.headers[std::move(key)] = std::move(value);
    }

    const auto it = head.headers.find("Content-Length");

    if (it != head.headers.end())
    {
      head.content_length = parse_content_length(it->second);
    }

    if (head.content_length > MAX_REQUEST_BODY_SIZE)
    {
      throw std::runtime_error("payload too large");
    }

    head.keep_alive = compute_keep_alive(head);
    return head;
  }

  task<std::string> Session::read_request_body(const ParsedRequestHead &head)
  {
    if (head.content_length == 0 || !method_allows_body(head.method))
    {
      co_return std::string{};
    }

    std::string body;
    body.reserve(head.content_length);

    if (!read_buffer_.empty())
    {
      const std::size_t take = std::min(head.content_length, read_buffer_.size());
      body.append(read_buffer_.data(), take);
      read_buffer_.erase(0, take);
    }

    while (body.size() < head.content_length)
    {
      std::array<std::byte, 8192> chunk{};

      const std::size_t need = std::min<std::size_t>(
          chunk.size(),
          head.content_length - body.size());

      const std::size_t n = co_await transport_->async_read(
          std::span<std::byte>(chunk.data(), need),
          timer_cancel_.token());

      if (n == 0)
      {
        throw std::runtime_error("unexpected EOF while reading body");
      }

      body.append(reinterpret_cast<const char *>(chunk.data()), n);
    }

    co_return body;
  }

  vix::http::Request Session::make_request(ParsedRequestHead head, std::string body)
  {
    vix::http::Request req;

    req.set_method(std::move(head.method));
    req.set_target(std::move(head.target));
    req.set_body(std::move(body));

    req.set_headers(std::move(head.headers));

    return req;
  }

  std::string Session::to_lower(std::string s)
  {
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c)
        {
          return static_cast<char>(std::tolower(c));
        });

    return s;
  }

  std::string Session::trim(std::string s)
  {
    std::size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first])))
    {
      ++first;
    }

    std::size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1])))
    {
      --last;
    }

    if (first == 0 && last == s.size())
    {
      return s;
    }

    return s.substr(first, last - first);
  }

  std::size_t Session::parse_content_length(const std::string &value)
  {
    if (value.empty())
    {
      throw std::runtime_error("empty Content-Length");
    }

    unsigned long long parsed = 0;

    try
    {
      std::size_t pos = 0;
      parsed = std::stoull(value, &pos, 10);

      if (pos != value.size())
      {
        throw std::runtime_error("invalid Content-Length");
      }
    }
    catch (const std::exception &)
    {
      throw std::runtime_error("invalid Content-Length");
    }

    if (parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()))
    {
      throw std::runtime_error("Content-Length overflow");
    }

    return static_cast<std::size_t>(parsed);
  }

  bool Session::method_allows_body(const std::string &method)
  {
    return equals_icase(method, "POST") ||
           equals_icase(method, "PUT") ||
           equals_icase(method, "PATCH") ||
           equals_icase(method, "DELETE");
  }

  bool Session::compute_keep_alive(const ParsedRequestHead &head)
  {
    auto it = head.headers.find("Connection");
    const std::string_view conn =
        (it == head.headers.end()) ? std::string_view{} : std::string_view{it->second};

    if (starts_with_icase(head.version, "HTTP/1.0"))
    {
      return equals_icase(conn, "keep-alive");
    }

    if (starts_with_icase(head.version, "HTTP/1.1"))
    {
      return !equals_icase(conn, "close");
    }

    return false;
  }

} // namespace vix::session
