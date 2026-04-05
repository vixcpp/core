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

#include <algorithm>
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

    inline bool icontains(std::string_view s, std::string_view needle)
    {
      if (needle.empty())
      {
        return true;
      }

      if (needle.size() > s.size())
      {
        return false;
      }

      for (std::size_t i = 0; i + needle.size() <= s.size(); ++i)
      {
        std::size_t j = 0;
        for (; j < needle.size(); ++j)
        {
          const unsigned char a = static_cast<unsigned char>(s[i + j]);
          const unsigned char b = static_cast<unsigned char>(needle[j]);

          if (static_cast<char>(std::tolower(a)) !=
              static_cast<char>(std::tolower(b)))
          {
            break;
          }
        }

        if (j == needle.size())
        {
          return true;
        }
      }

      return false;
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

    inline bool is_close_connection(std::string_view value)
    {
      return equals_icase(value, "close");
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

    inline std::string_view strip_query_view(std::string_view target)
    {
      const auto q = target.find('?');
      if (q == std::string_view::npos)
      {
        return target;
      }

      return target.substr(0, q);
    }
  } // namespace

  const std::regex Session::XSS_PATTERN(
      R"(<script.*?>.*?</script>)",
      std::regex::icase);

  const std::regex Session::SQL_PATTERN(
      R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))",
      std::regex::icase);

  Session::Session(std::unique_ptr<tcp_stream> stream,
                   vix::router::Router &router,
                   const vix::config::Config &config,
                   std::shared_ptr<vix::executor::IExecutor> executor)
      : stream_(std::move(stream)),
        router_(router),
        config_(config),
        executor_(std::move(executor)),
        read_buffer_(),
        io_context_(nullptr),
        timer_cancel_()
  {
    if (!executor_)
    {
      throw std::invalid_argument("Session requires a valid executor");
    }
  }

  task<void> Session::run()
  {
    try
    {
      while (stream_ && stream_->is_open())
      {
        auto maybe_req = co_await read_request();
        if (!maybe_req.has_value())
        {
          break;
        }

        co_await dispatch_request(std::move(*maybe_req));

        if (!stream_ || !stream_->is_open())
        {
          break;
        }
      }
    }
    catch (const std::exception &e)
    {
      log().log(Logger::Level::Error,
                "[session] fatal session error: {}",
                e.what());
    }

    co_await close_stream_gracefully();
    co_return;
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
    const auto code = e.code();

    if (code == std::errc::operation_canceled ||
        code == std::errc::broken_pipe ||
        code == std::errc::connection_reset ||
        code == std::errc::connection_aborted ||
        code == std::errc::timed_out)
    {
      return true;
    }

#ifdef ECANCELED
    if (code.value() == ECANCELED)
    {
      return true;
    }
#endif

#ifdef EPIPE
    if (code.value() == EPIPE)
    {
      return true;
    }
#endif

#ifdef ECONNRESET
    if (code.value() == ECONNRESET)
    {
      return true;
    }
#endif

#ifdef ECONNABORTED
    if (code.value() == ECONNABORTED)
    {
      return true;
    }
#endif

#ifdef ETIMEDOUT
    if (code.value() == ETIMEDOUT)
    {
      return true;
    }
#endif

    const std::string msg = to_lower(e.what());
    if (msg.find("end of file") != std::string::npos ||
        msg.find("eof") != std::string::npos)
    {
      return true;
    }

    return false;
  }

  task<std::optional<vix::vhttp::Request>> Session::read_request()
  {
    if (!stream_ || !stream_->is_open())
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
        co_await send_error(vix::vhttp::BAD_REQUEST, "Request blocked (security)");
        co_return std::nullopt;
      }

      if (req.body().size() > MAX_REQUEST_BODY_SIZE)
      {
        co_await send_error(vix::vhttp::PAYLOAD_TOO_LARGE, "Request too large");
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
        log().log(Logger::Level::Debug,
                  "[session] client disconnected: {}",
                  e.what());
        co_return std::nullopt;
      }

      log().log(Logger::Level::Error,
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

      log().log(Logger::Level::Error,
                "[session] request parse error: {}",
                e.what());

      malformed_request = true;
    }

    if (malformed_request)
    {
      co_await send_error(vix::vhttp::BAD_REQUEST, "Malformed HTTP request");
    }

    co_return std::nullopt;
  }

  task<void> Session::dispatch_request(vix::vhttp::Request req)
  {
#ifdef VIX_BENCH_MODE
    {
      const std::string_view method{req.method()};
      const std::string_view target = strip_query_view(req.target());

      if (equals_icase(method, "GET") && target == "/bench")
      {
        static constexpr std::string_view kBenchBody = "OK";
        static constexpr std::string_view kBenchResponse =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: keep-alive\r\n"
            "Server: Vix.cpp\r\n"
            "\r\n"
            "OK";

        const std::string conn = req.header("Connection");
        const bool close_requested = !conn.empty() && is_close_connection(conn);

        if (!close_requested)
        {
          std::size_t written = 0;
          while (written < kBenchResponse.size())
          {
            const std::byte *ptr =
                reinterpret_cast<const std::byte *>(kBenchResponse.data() + written);

            const std::size_t n = co_await stream_->async_write(
                std::span<const std::byte>(ptr, kBenchResponse.size() - written),
                timer_cancel_.token());

            if (n == 0)
            {
              break;
            }

            written += n;
          }

          co_return;
        }

        static constexpr std::string_view kBenchResponseClose =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n"
            "Server: Vix.cpp\r\n"
            "\r\n"
            "OK";

        std::size_t written = 0;
        while (written < kBenchResponseClose.size())
        {
          const std::byte *ptr =
              reinterpret_cast<const std::byte *>(kBenchResponseClose.data() + written);

          const std::size_t n = co_await stream_->async_write(
              std::span<const std::byte>(ptr, kBenchResponseClose.size() - written),
              timer_cancel_.token());

          if (n == 0)
          {
            break;
          }

          written += n;
        }

        co_await close_stream_gracefully();
        co_return;
      }
    }
#endif

    vix::vhttp::Response res;

    try
    {
      (void)executor_;
      (void)co_await router_.handle_request(req, res);
    }
    catch (const std::exception &ex)
    {
      log().log(Logger::Level::Error,
                "[router] exception: {}",
                ex.what());

      vix::vhttp::Response::error_response(
          res,
          vix::vhttp::INTERNAL_ERROR,
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

  task<void> Session::send_response(vix::vhttp::Response res)
  {
    if (!stream_ || !stream_->is_open())
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
      std::string wire = res.to_http_string();

      std::size_t written = 0;
      while (written < wire.size())
      {
        const std::byte *ptr =
            reinterpret_cast<const std::byte *>(wire.data() + written);

        const std::size_t n = co_await stream_->async_write(
            std::span<const std::byte>(ptr, wire.size() - written),
            timer_cancel_.token());

        if (n == 0)
        {
          break;
        }

        written += n;
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
    catch (const std::exception &e)
    {
#ifndef VIX_BENCH_MODE
      if (!config_.isBenchMode())
      {
        cancel_timer();
      }
#endif

      log().log(Logger::Level::Error,
                "[session] write error: {}",
                e.what());

      must_close = true;
    }

    if (must_close)
    {
      co_await close_stream_gracefully();
    }

    co_return;
  }

  task<void> Session::send_error(int status, const std::string &msg)
  {
    vix::vhttp::Response res;
    vix::vhttp::Response::error_response(res, status, msg);
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

    if (!stream_)
    {
      co_return;
    }

    try
    {
      if (stream_->is_open())
      {
        stream_->close();
      }
    }
    catch (...)
    {
    }

    co_return;
  }

  bool Session::waf_check_request(const vix::vhttp::Request &req)
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
        icontains(target, "script") ||
        icontains(target, "union") ||
        icontains(target, "select") ||
        icontains(target, "drop");

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
        icontains(body, "union") ||
        icontains(body, "select") ||
        icontains(body, "drop") ||
        icontains(body, "insert") ||
        icontains(body, "delete") ||
        icontains(body, "update");

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
        std::string header = read_buffer_.substr(0, pos);
        read_buffer_.erase(0, pos);
        co_return header;
      }

      if (read_buffer_.size() > MAX_HEADER_BYTES)
      {
        throw std::runtime_error("HTTP header too large");
      }

      std::array<std::byte, 8192> chunk{};
      const std::size_t n = co_await stream_->async_read(
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
    if (block.size() >= 4 &&
        block.substr(block.size() - 4) == "\r\n\r\n")
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

      const std::size_t n = co_await stream_->async_read(
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

  vix::vhttp::Request Session::make_request(ParsedRequestHead head, std::string body)
  {
    vix::vhttp::Request req;

    req.set_method(std::move(head.method));
    req.set_target(std::move(head.target));
    req.set_body(std::move(body));

    for (auto &[k, v] : head.headers)
    {
      req.set_header(k, v);
    }

    return req;
  }

  std::string Session::to_lower(std::string s)
  {
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c)
        { return static_cast<char>(std::tolower(c)); });

    return s;
  }

  std::string Session::trim(std::string s)
  {
    auto not_space = [](unsigned char c)
    { return !std::isspace(c); };

    while (!s.empty() && !not_space(static_cast<unsigned char>(s.front())))
    {
      s.erase(s.begin());
    }

    while (!s.empty() && !not_space(static_cast<unsigned char>(s.back())))
    {
      s.pop_back();
    }

    return s;
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
    const std::string m = to_lower(method);

    return m == "post" ||
           m == "put" ||
           m == "patch" ||
           m == "delete";
  }

  bool Session::compute_keep_alive(const ParsedRequestHead &head)
  {
    auto it = head.headers.find("Connection");
    const std::string conn = (it == head.headers.end()) ? std::string{} : to_lower(it->second);

    if (starts_with_icase(head.version, "HTTP/1.0"))
    {
      return conn == "keep-alive";
    }

    if (starts_with_icase(head.version, "HTTP/1.1"))
    {
      return conn != "close";
    }

    return false;
  }

} // namespace vix::session
