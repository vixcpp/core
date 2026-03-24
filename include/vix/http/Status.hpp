/**
 * @file Status.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_STATUS_HPP
#define VIX_STATUS_HPP

#include <cassert>
#include <string>
#include <string_view>

namespace vix::vhttp
{
  /** @brief Common HTTP informational status codes. */
  inline constexpr int CONTINUE = 100;
  inline constexpr int SWITCHING_PROTOCOLS = 101;
  inline constexpr int PROCESSING = 102;
  inline constexpr int EARLY_HINTS = 103;

  /** @brief Common HTTP success status codes. */
  inline constexpr int OK = 200;
  inline constexpr int CREATED = 201;
  inline constexpr int ACCEPTED = 202;
  inline constexpr int NON_AUTHORITATIVE_INFORMATION = 203;
  inline constexpr int NO_CONTENT = 204;
  inline constexpr int RESET_CONTENT = 205;
  inline constexpr int PARTIAL_CONTENT = 206;

  /** @brief Common HTTP redirection status codes. */
  inline constexpr int MULTIPLE_CHOICES = 300;
  inline constexpr int MOVED_PERMANENTLY = 301;
  inline constexpr int FOUND = 302;
  inline constexpr int SEE_OTHER = 303;
  inline constexpr int NOT_MODIFIED = 304;
  inline constexpr int TEMPORARY_REDIRECT = 307;
  inline constexpr int PERMANENT_REDIRECT = 308;

  /** @brief Common HTTP client error status codes. */
  inline constexpr int BAD_REQUEST = 400;
  inline constexpr int UNAUTHORIZED = 401;
  inline constexpr int PAYMENT_REQUIRED = 402;
  inline constexpr int FORBIDDEN = 403;
  inline constexpr int NOT_FOUND = 404;
  inline constexpr int METHOD_NOT_ALLOWED = 405;
  inline constexpr int NOT_ACCEPTABLE = 406;
  inline constexpr int PROXY_AUTHENTICATION_REQUIRED = 407;
  inline constexpr int REQUEST_TIMEOUT = 408;
  inline constexpr int CONFLICT = 409;
  inline constexpr int GONE = 410;
  inline constexpr int LENGTH_REQUIRED = 411;
  inline constexpr int PRECONDITION_FAILED = 412;
  inline constexpr int PAYLOAD_TOO_LARGE = 413;
  inline constexpr int URI_TOO_LONG = 414;
  inline constexpr int UNSUPPORTED_MEDIA_TYPE = 415;
  inline constexpr int RANGE_NOT_SATISFIABLE = 416;
  inline constexpr int EXPECTATION_FAILED = 417;
  inline constexpr int MISDIRECTED_REQUEST = 421;
  inline constexpr int UNPROCESSABLE_ENTITY = 422;
  inline constexpr int TOO_EARLY = 425;
  inline constexpr int UPGRADE_REQUIRED = 426;
  inline constexpr int TOO_MANY_REQUESTS = 429;

  /** @brief Common HTTP server error status codes. */
  inline constexpr int INTERNAL_ERROR = 500;
  inline constexpr int NOT_IMPLEMENTED = 501;
  inline constexpr int BAD_GATEWAY = 502;
  inline constexpr int SERVICE_UNAVAILABLE = 503;
  inline constexpr int GATEWAY_TIMEOUT = 504;
  inline constexpr int HTTP_VERSION_NOT_SUPPORTED = 505;

  /**
   * @brief Return true if the given HTTP status code is in the valid range.
   */
  inline constexpr bool is_valid_status(int code) noexcept
  {
    return code >= 100 && code <= 599;
  }

  /**
   * @brief Normalize a numeric HTTP status code.
   *
   * Returns the input code if valid, otherwise falls back to 500.
   */
  inline constexpr int normalize_status(int code) noexcept
  {
    if (is_valid_status(code))
      return code;

#ifndef NDEBUG
    assert(false && "Invalid HTTP status code: must be between 100 and 599");
#endif

    return INTERNAL_ERROR;
  }

  /**
   * @brief Return the canonical reason phrase for a status code.
   */
  inline constexpr std::string_view reason_phrase(int code) noexcept
  {
    switch (code)
    {
    case CONTINUE:
      return "Continue";
    case SWITCHING_PROTOCOLS:
      return "Switching Protocols";
    case PROCESSING:
      return "Processing";
    case EARLY_HINTS:
      return "Early Hints";

    case OK:
      return "OK";
    case CREATED:
      return "Created";
    case ACCEPTED:
      return "Accepted";
    case NON_AUTHORITATIVE_INFORMATION:
      return "Non-Authoritative Information";
    case NO_CONTENT:
      return "No Content";
    case RESET_CONTENT:
      return "Reset Content";
    case PARTIAL_CONTENT:
      return "Partial Content";

    case MULTIPLE_CHOICES:
      return "Multiple Choices";
    case MOVED_PERMANENTLY:
      return "Moved Permanently";
    case FOUND:
      return "Found";
    case SEE_OTHER:
      return "See Other";
    case NOT_MODIFIED:
      return "Not Modified";
    case TEMPORARY_REDIRECT:
      return "Temporary Redirect";
    case PERMANENT_REDIRECT:
      return "Permanent Redirect";

    case BAD_REQUEST:
      return "Bad Request";
    case UNAUTHORIZED:
      return "Unauthorized";
    case PAYMENT_REQUIRED:
      return "Payment Required";
    case FORBIDDEN:
      return "Forbidden";
    case NOT_FOUND:
      return "Not Found";
    case METHOD_NOT_ALLOWED:
      return "Method Not Allowed";
    case NOT_ACCEPTABLE:
      return "Not Acceptable";
    case PROXY_AUTHENTICATION_REQUIRED:
      return "Proxy Authentication Required";
    case REQUEST_TIMEOUT:
      return "Request Timeout";
    case CONFLICT:
      return "Conflict";
    case GONE:
      return "Gone";
    case LENGTH_REQUIRED:
      return "Length Required";
    case PRECONDITION_FAILED:
      return "Precondition Failed";
    case PAYLOAD_TOO_LARGE:
      return "Payload Too Large";
    case URI_TOO_LONG:
      return "URI Too Long";
    case UNSUPPORTED_MEDIA_TYPE:
      return "Unsupported Media Type";
    case RANGE_NOT_SATISFIABLE:
      return "Range Not Satisfiable";
    case EXPECTATION_FAILED:
      return "Expectation Failed";
    case MISDIRECTED_REQUEST:
      return "Misdirected Request";
    case UNPROCESSABLE_ENTITY:
      return "Unprocessable Entity";
    case TOO_EARLY:
      return "Too Early";
    case UPGRADE_REQUIRED:
      return "Upgrade Required";
    case TOO_MANY_REQUESTS:
      return "Too Many Requests";

    case INTERNAL_ERROR:
      return "Internal Server Error";
    case NOT_IMPLEMENTED:
      return "Not Implemented";
    case BAD_GATEWAY:
      return "Bad Gateway";
    case SERVICE_UNAVAILABLE:
      return "Service Unavailable";
    case GATEWAY_TIMEOUT:
      return "Gateway Timeout";
    case HTTP_VERSION_NOT_SUPPORTED:
      return "HTTP Version Not Supported";

    default:
      return "Unknown";
    }
  }

  /**
   * @brief Return a human-readable HTTP status line fragment.
   *
   * Example: "404 Not Found"
   */
  inline std::string status_to_string(int code)
  {
    code = normalize_status(code);
    return std::to_string(code) + " " + std::string(reason_phrase(code));
  }

} // namespace vix::vhttp

#endif // VIX_STATUS_HPP
