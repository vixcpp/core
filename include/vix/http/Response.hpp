/**
 * @file Response.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vix/http/Status.hpp>
#include <vix/json/json.hpp>

namespace vix::http
{
  template <class T>
  concept JsonSerializable =
      requires(const T &value) {
        vix::json::Json(value);
      } ||
      requires(const T &value) {
        value.dump();
      };

  /**
   * @brief Minimal native HTTP response object for Vix.
   *
   * This type is independent of Boost.Beast and is intended to be serialized
   * directly by the Vix async HTTP session layer.
   */
  class Response
  {
  public:
    using HeaderMap = std::unordered_map<std::string, std::string>;

    Response() = default;

    explicit Response(int status)
        : status_(status)
    {
    }

    Response(int status, std::string body)
        : status_(status),
          body_(std::move(body))
    {
    }

    Response(const Response &) = default;
    Response &operator=(const Response &) = default;
    Response(Response &&) noexcept = default;
    Response &operator=(Response &&) noexcept = default;
    ~Response() = default;

    /** @brief Return the numeric HTTP status code. */
    int status() const noexcept
    {
      return status_;
    }

    /** @brief Set the numeric HTTP status code. */
    void set_status(int status) noexcept
    {
      status_ = status;
    }

    /** @brief Return the response body. */
    const std::string &body() const noexcept
    {
      return body_;
    }

    /** @brief Return a mutable response body. */
    std::string &body() noexcept
    {
      return body_;
    }

    /** @brief Replace the response body. */
    void set_body(std::string body)
    {
      body_ = std::move(body);
    }

    /** @brief Return all response headers. */
    const HeaderMap &headers() const noexcept
    {
      return headers_;
    }

    /** @brief Return mutable response headers. */
    HeaderMap &headers() noexcept
    {
      return headers_;
    }

    /** @brief Replace all response headers. */
    void set_headers(HeaderMap headers)
    {
      headers_ = std::move(headers);
    }

    /** @brief Return true if the response already contains a header. */
    bool has_header(std::string_view name) const
    {
      return headers_.find(std::string(name)) != headers_.end();
    }

    /** @brief Return a header value or an empty string if missing. */
    std::string header(std::string_view name) const
    {
      auto it = headers_.find(std::string(name));
      return it == headers_.end() ? std::string{} : it->second;
    }

    /** @brief Set or replace one header. */
    void set_header(std::string name, std::string value)
    {
      headers_[std::move(name)] = std::move(value);
    }

    /** @brief Remove a header if present. */
    void remove_header(std::string_view name)
    {
      if (const auto it = headers_.find(std::string(name)); it != headers_.end())
      {
        headers_.erase(it);
      }
    }

    /** @brief Remove all headers. */
    void clear_headers() noexcept
    {
      headers_.clear();
    }

    /** @brief Return whether the response should close the connection. */
    bool should_close() const noexcept
    {
      return close_;
    }

    /** @brief Set whether the connection should be closed after sending. */
    void set_should_close(bool close) noexcept
    {
      close_ = close;
    }

    /** @brief Return the HTTP reason phrase if explicitly set. */
    const std::string &reason() const noexcept
    {
      return reason_;
    }

    /** @brief Set a custom HTTP reason phrase. */
    void set_reason(std::string reason)
    {
      reason_ = std::move(reason);
    }

    /** @brief Clear any custom reason phrase. */
    void clear_reason() noexcept
    {
      reason_.clear();
    }

    /** @brief Return the HTTP version string. */
    const std::string &version() const noexcept
    {
      return version_;
    }

    /** @brief Set the HTTP version string, e.g. "HTTP/1.1". */
    void set_version(std::string version)
    {
      version_ = std::move(version);
    }

    /** @brief Return the serialized response as raw HTTP text. */
    std::string to_http_string() const
    {
      Response copy = *this;
      copy.apply_default_headers();

      std::ostringstream oss;
      oss << copy.version_ << ' ' << copy.status_ << ' ' << copy.status_text() << "\r\n";

      for (const auto &[name, value] : copy.headers_)
      {
        oss << name << ": " << value << "\r\n";
      }

      oss << "\r\n";
      oss << copy.body_;
      return oss.str();
    }

    /** @brief Convert a JSON-like value into a string using either vix::json or a .dump() API. */
    template <class J>
    static std::string to_json_string(const J &j)
    {
      if constexpr (std::is_same_v<std::decay_t<J>, vix::json::Json>)
      {
        return vix::json::dumps_compact(j);
      }
      else if constexpr (requires { j.dump(); })
      {
        return j.dump();
      }
      else
      {
        return vix::json::dumps_compact(vix::json::Json(j));
      }
    }

    /** @brief Return the current time formatted as an HTTP date (RFC 7231) in GMT. */
    static std::string http_date_now() noexcept
    {
      using clock = std::chrono::system_clock;
      const auto now = clock::now();
      const std::time_t t = clock::to_time_t(now);

      std::tm tm{};
#if defined(_WIN32)
      ::gmtime_s(&tm, &t);
#elif defined(__unix__) || defined(__APPLE__)
      ::gmtime_r(&t, &tm);
#else
      tm = *std::gmtime(&t);
#endif

      std::ostringstream oss;
      oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
      return oss.str();
    }

    /** @brief Return an HTTP date cached for the current second. */
    static const std::string &cached_http_date_now() noexcept
    {
      using clock = std::chrono::system_clock;

      static std::string cached = http_date_now();
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
        cached = http_date_now();
        last_tick = clock::now();
      }

      lock.clear(std::memory_order_release);
      return cached;
    }

    /** @brief Apply common headers (Server, Date) if absent. */
    static void common_headers(Response &res) noexcept
    {
      if (!res.has_header("Server"))
      {
        res.set_header("Server", "Vix.cpp");
      }

      if (!res.has_header("Date"))
      {
        res.set_header("Date", cached_http_date_now());
      }
    }

    /** @brief Return true if the response already contains the given header. */
    static bool has_header(const Response &res, std::string_view name) noexcept
    {
      return res.has_header(name);
    }

    /** @brief Create a JSON response with {"message": "..."} and the given status. */
    static void create_response(
        Response &res,
        int status,
        std::string_view message,
        std::string_view content_type = "application/json; charset=utf-8")
    {
      res.set_status(status);

      if (!res.has_header("Content-Type"))
      {
        res.set_header("Content-Type", std::string(content_type));
      }

      res.set_body(vix::json::dumps_compact(
          vix::json::Json{{"message", std::string(message)}}));
      res.apply_default_headers();
    }

    /** @brief Send an error response with a JSON {"message": "..."} body. */
    static void error_response(
        Response &res,
        int status,
        std::string_view message)
    {
      create_response(res, status, message);
    }

    /** @brief Send a 200 OK JSON {"message": "..."} response. */
    static void success_response(
        Response &res,
        std::string_view message)
    {
      create_response(res, 200, message);
    }

    /** @brief Send a 204 No Content response with a JSON {"message": "..."} body. */
    static void no_content_response(
        Response &res,
        std::string_view message = "No Content")
    {
      (void)message;

      res.set_status(204);
      res.set_body("");
      res.apply_default_headers();
    }

    /** @brief Send a 302 Found redirect response with a JSON body and Location header. */
    static void redirect_response(
        Response &res,
        std::string_view location)
    {
      res.set_status(302);
      res.set_header("Location", std::string(location));

      if (!res.has_header("Content-Type"))
      {
        res.set_header("Content-Type", "application/json; charset=utf-8");
      }

      res.set_body(vix::json::dumps_compact(
          vix::json::Json{
              {"message", std::string("Redirecting to ") + std::string(location)}}));

      res.apply_default_headers();
    }

    /** @brief Send a JSON response with the given status. */
    template <class J>
      requires JsonSerializable<J>
    static void json_response(
        Response &res,
        const J &data,
        int status = 200)
    {
      res.set_status(status);

      if (!res.has_header("Content-Type"))
      {
        res.set_header("Content-Type", "application/json; charset=utf-8");
      }

      res.set_body(to_json_string(data));
      res.apply_default_headers();
    }

    /** @brief Send a plain text response with the given status. */
    static void text_response(
        Response &res,
        std::string_view data,
        int status = 200)
    {
      res.set_status(status);

      if (!res.has_header("Content-Type"))
      {
        res.set_header("Content-Type", "text/plain; charset=utf-8");
      }

      res.set_body(std::string(data));
      res.apply_default_headers();
    }

  private:
    void apply_default_headers()
    {
      common_headers(*this);

      if (!has_header("Content-Length"))
      {
        set_header("Content-Length", std::to_string(body_.size()));
      }

      if (!has_header("Connection"))
      {
        set_header("Connection", close_ ? "close" : "keep-alive");
      }
    }

    std::string status_text() const
    {
      if (!reason_.empty())
      {
        return reason_;
      }

      switch (status_)
      {
      case 100:
        return "Continue";
      case 101:
        return "Switching Protocols";
      case 102:
        return "Processing";
      case 103:
        return "Early Hints";
      case 200:
        return "OK";
      case 201:
        return "Created";
      case 202:
        return "Accepted";
      case 203:
        return "Non-Authoritative Information";
      case 204:
        return "No Content";
      case 205:
        return "Reset Content";
      case 206:
        return "Partial Content";
      case 300:
        return "Multiple Choices";
      case 301:
        return "Moved Permanently";
      case 302:
        return "Found";
      case 303:
        return "See Other";
      case 304:
        return "Not Modified";
      case 307:
        return "Temporary Redirect";
      case 308:
        return "Permanent Redirect";
      case 400:
        return "Bad Request";
      case 401:
        return "Unauthorized";
      case 403:
        return "Forbidden";
      case 404:
        return "Not Found";
      case 405:
        return "Method Not Allowed";
      case 406:
        return "Not Acceptable";
      case 408:
        return "Request Timeout";
      case 409:
        return "Conflict";
      case 410:
        return "Gone";
      case 411:
        return "Length Required";
      case 412:
        return "Precondition Failed";
      case 413:
        return "Payload Too Large";
      case 414:
        return "URI Too Long";
      case 415:
        return "Unsupported Media Type";
      case 422:
        return "Unprocessable Entity";
      case 425:
        return "Too Early";
      case 426:
        return "Upgrade Required";
      case 429:
        return "Too Many Requests";
      case 500:
        return "Internal Server Error";
      case 501:
        return "Not Implemented";
      case 502:
        return "Bad Gateway";
      case 503:
        return "Service Unavailable";
      case 504:
        return "Gateway Timeout";
      case 505:
        return "HTTP Version Not Supported";
      default:
        return "Unknown";
      }
    }

  private:
    int status_{200};
    std::string version_{"HTTP/1.1"};
    std::string reason_{};
    std::string body_{};
    HeaderMap headers_{};
    bool close_{false};
  };

} // namespace vix::http

#endif // VIX_RESPONSE_HPP
