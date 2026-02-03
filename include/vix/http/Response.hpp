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

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/Status.hpp>

#if __has_include(<vix/json/json.hpp>)
#include <vix/json/json.hpp>
#define VIX_CORE_HAS_VIX_JSON 1
#else
#define VIX_CORE_HAS_VIX_JSON 0
#endif

namespace vix::vhttp
{
  namespace http = boost::beast::http;

#if __cpp_concepts
  template <class T>
  concept HasDump = requires(const T &x) { x.dump(); };

  template <class T>
  concept SupportedJson =
      std::is_same_v<std::decay_t<T>, nlohmann::json> ||
#if VIX_CORE_HAS_VIX_JSON
      std::is_same_v<std::decay_t<T>, vix::json::Json> ||
#endif
      HasDump<T>;
#else
  template <class, class = void>
  struct has_dump : std::false_type
  {
  };

  template <class T>
  struct has_dump<T, std::void_t<decltype(std::declval<const T &>().dump())>> : std::true_type
  {
  };

  template <class T>
  struct is_supported_json
      : std::bool_constant<
            std::is_same_v<std::decay_t<T>, nlohmann::json>
#if VIX_CORE_HAS_VIX_JSON
            || std::is_same_v<std::decay_t<T>, vix::json::Json>
#endif
            || has_dump<T>::value>
  {
  };
#endif

  /** @brief Convert a JSON-like value into a string using either vix::json or a .dump() API. */
  template <class J>
  inline std::string to_json_string(const J &j)
  {
#if __cpp_concepts
    static_assert(SupportedJson<J>,
                  "Response::to_json_string(): J must be nlohmann::json, vix::json::Json, or expose .dump()");
#else
    static_assert(is_supported_json<J>::value,
                  "Response::to_json_string(): J must be nlohmann::json, vix::json::Json, or expose .dump()");
#endif

#if VIX_CORE_HAS_VIX_JSON
    if constexpr (std::is_same_v<std::decay_t<J>, vix::json::Json>)
    {
      return vix::json::dumps(j);
    }
    else
#endif
    {
      if constexpr (
#if __cpp_concepts
          HasDump<J>
#else
          has_dump<J>::value
#endif
      )
      {
        return j.dump();
      }
      else
      {
        return j.dump();
      }
    }
  }

  /** @brief Return the current time formatted as an HTTP date (RFC 7231) in GMT. */
  inline std::string http_date_now() noexcept
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

  template <int Code>
  struct status_code_in_range
  {
    static constexpr bool value = (Code >= 100) && (Code <= 599);
  };

  /** @brief Static helpers for building Boost.Beast HTTP responses (JSON, text, errors, redirects). */
  class Response
  {
  public:
    /** @brief Apply common headers (Server, Date) to a response. */
    static void common_headers(http::response<http::string_body> &res) noexcept
    {
      res.set(http::field::server, "Vix/master");
      res.set(http::field::date, http_date_now());
    }

    /** @brief Create a JSON response with {"message": "..."} and the given status. */
    static void create_response(
        http::response<http::string_body> &res,
        http::status status,
        std::string_view message,
        std::string_view content_type = "application/json")
    {
      res.result(status);

      if (res.find(http::field::content_type) == res.end())
      {
        res.set(http::field::content_type, std::string(content_type));
      }

      res.body() = nlohmann::json{{"message", message}}.dump();
      common_headers(res);
      res.prepare_payload();
    }

    /** @brief Create a JSON response with {"message": "..."} and a numeric status code. */
    static void create_response(
        http::response<http::string_body> &res,
        int status,
        std::string_view message,
        std::string_view content_type = "application/json")
    {
      create_response(res, to_status(status), message, content_type);
    }

    /** @brief Create a JSON response with {"message": "..."} and a compile-time status code. */
    template <int Code>
    static void create_response(
        http::response<http::string_body> &res,
        std::string_view message,
        std::string_view content_type = "application/json")
    {
      static_assert(status_code_in_range<Code>::value, "HTTP status code must be between 100 and 599");
      create_response(res, static_cast<http::status>(Code), message, content_type);
    }

    /** @brief Return true if the response already contains the given header. */
    static bool has_header(const http::response<http::string_body> &res, http::field f) noexcept
    {
      return res.find(f) != res.end();
    }

    /** @brief Send an error response with a JSON {"message": "..."} body. */
    static void error_response(
        http::response<http::string_body> &res,
        http::status status,
        std::string_view message)
    {
      create_response(res, status, message);
    }

    /** @brief Send an error response with a numeric status code. */
    static void error_response(
        http::response<http::string_body> &res,
        int status,
        std::string_view message)
    {
      create_response(res, to_status(status), message);
    }

    /** @brief Send an error response with a compile-time status code. */
    template <int Code>
    static void error_response(
        http::response<http::string_body> &res,
        std::string_view message)
    {
      static_assert(status_code_in_range<Code>::value, "HTTP status code must be between 100 and 599");
      create_response(res, static_cast<http::status>(Code), message);
    }

    /** @brief Send a 200 OK JSON {"message": "..."} response. */
    static void success_response(
        http::response<http::string_body> &res,
        std::string_view message)
    {
      create_response(res, http::status::ok, message);
    }

    /** @brief Send a 204 No Content response with a JSON {"message": "..."} body. */
    static void no_content_response(
        http::response<http::string_body> &res,
        std::string_view message = "No Content")
    {
      res.result(http::status::no_content);

      if (res.find(http::field::content_type) == res.end())
      {
        res.set(http::field::content_type, "application/json; charset=utf-8");
      }

      res.body() = nlohmann::json{{"message", message}}.dump();
      common_headers(res);
      res.prepare_payload();
    }

    /** @brief Send a 302 Found redirect response with a JSON body and Location header. */
    static void redirect_response(
        http::response<http::string_body> &res,
        std::string_view location)
    {
      res.result(http::status::found);
      res.set(http::field::location, std::string(location));

      if (res.find(http::field::content_type) == res.end())
      {
        res.set(http::field::content_type, "application/json; charset=utf-8");
      }

      res.body() = nlohmann::json{
          {"message", std::string("Redirecting to ") + std::string(location)}}
                       .dump();
      common_headers(res);
      res.prepare_payload();
    }

    /** @brief Send a JSON response with the given status. */
    template <class J>
#if __cpp_concepts
      requires SupportedJson<J>
#endif
    static void json_response(
        http::response<http::string_body> &res,
        const J &data,
        http::status status = http::status::ok)
    {
      res.result(status);

      if (res.find(http::field::content_type) == res.end())
      {
        res.set(http::field::content_type, "application/json; charset=utf-8");
      }

      res.body() = to_json_string(data);
      common_headers(res);
      res.prepare_payload();
    }

    /** @brief Send a JSON response using a numeric status code. */
    template <class J>
#if __cpp_concepts
      requires SupportedJson<J>
#endif
    static void json_response(http::response<http::string_body> &res,
                              const J &data,
                              int status)
    {
      json_response(res, data, to_status(status));
    }

    /** @brief Send a JSON response using a compile-time status code. */
    template <int Code, class J>
#if __cpp_concepts
      requires SupportedJson<J>
#endif
    static void json_response(
        http::response<http::string_body> &res,
        const J &data)
    {
      static_assert(status_code_in_range<Code>::value, "HTTP status code must be between 100 and 599");
      json_response(res, data, static_cast<http::status>(Code));
    }

    /** @brief Send a plain text response with the given status. */
    static void text_response(
        http::response<http::string_body> &res,
        std::string_view data,
        http::status status = http::status::ok)
    {
      res.result(status);

      if (res.find(http::field::content_type) == res.end())
      {
        res.set(http::field::content_type, "text/plain; charset=utf-8");
      }

      res.body() = std::string(data);
      common_headers(res);
      res.prepare_payload();
    }

    /** @brief Send a plain text response using a numeric status code. */
    static void text_response(
        http::response<http::string_body> &res,
        std::string_view data,
        int status)
    {
      text_response(res, data, to_status(status));
    }

    /** @brief Send a plain text response using a compile-time status code. */
    template <int Code>
    static void text_response(
        http::response<http::string_body> &res,
        std::string_view data)
    {
      static_assert(status_code_in_range<Code>::value, "HTTP status code must be between 100 and 599");
      text_response(res, data, static_cast<http::status>(Code));
    }
  };

} // namespace vix::vhttp

#endif // VIX_RESPONSE_HPP
