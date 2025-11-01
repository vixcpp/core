#ifndef VIX_STATUS_HPP
#define VIX_STATUS_HPP

/**
 * @file Status.hpp
 * @brief HTTP status constants and safe conversion helpers for Vix.cpp.
 *
 * @details
 * This header defines numeric constants for standard HTTP status codes
 * (e.g. `vix::vhttp::OK = 200`, `vix::vhttp::NOT_FOUND = 404`) and provides
 * a `to_status(int)` helper to safely convert them to Boost.Beast enums.
 *
 * ### Example
 * ```cpp
 * using namespace vix::vhttp;
 *
 * // Direct usage (int constants)
 * res.result(OK);
 *
 * // Safe conversion (if you need http::status)
 * res.result(to_status(OK));
 * ```
 */

#include <boost/beast/http/status.hpp>
#include <cassert>
#include <string>

namespace vix::vhttp
{
    namespace http = boost::beast::http;

    // ----------------------------------------------------------
    // Numeric HTTP constants (expressive + constexpr)
    // ----------------------------------------------------------
    inline constexpr int OK = 200;
    inline constexpr int CREATED = 201;
    inline constexpr int ACCEPTED = 202;
    inline constexpr int NO_CONTENT = 204;

    inline constexpr int MOVED_PERMANENTLY = 301;
    inline constexpr int FOUND = 302;

    inline constexpr int BAD_REQUEST = 400;
    inline constexpr int UNAUTHORIZED = 401;
    inline constexpr int FORBIDDEN = 403;
    inline constexpr int NOT_FOUND = 404;
    inline constexpr int CONFLICT = 409;

    inline constexpr int INTERNAL_ERROR = 500;
    inline constexpr int NOT_IMPLEMENTED = 501;
    inline constexpr int BAD_GATEWAY = 502;
    inline constexpr int SERVICE_UNAVAILABLE = 503;

    // ----------------------------------------------------------
    // Safe converter: int → Boost.Beast status
    // ----------------------------------------------------------
    /**
     * @brief Convert a numeric status code to a `http::status` enum.
     *
     * @param code Integer (e.g. 200, 404).
     * @return `http::status` equivalent.
     *
     * @note If the code is outside [100–599], it asserts in Debug
     * and falls back to `internal_server_error` in Release.
     */
    inline constexpr http::status to_status(int code) noexcept
    {
        if (code >= 100 && code <= 599)
            return static_cast<http::status>(code);

#ifndef NDEBUG
        assert(false && "Invalid HTTP status code: must be between 100 and 599");
#endif
        return http::status::internal_server_error;
    }

    // ----------------------------------------------------------
    // Optional helper: convert enum to readable string (for logs)
    // ----------------------------------------------------------
    inline std::string status_to_string(int code)
    {
        switch (code)
        {
        case OK:
            return "200 OK";
        case CREATED:
            return "201 Created";
        case ACCEPTED:
            return "202 Accepted";
        case NO_CONTENT:
            return "204 No Content";
        case BAD_REQUEST:
            return "400 Bad Request";
        case UNAUTHORIZED:
            return "401 Unauthorized";
        case FORBIDDEN:
            return "403 Forbidden";
        case NOT_FOUND:
            return "404 Not Found";
        case CONFLICT:
            return "409 Conflict";
        case INTERNAL_ERROR:
            return "500 Internal Server Error";
        case NOT_IMPLEMENTED:
            return "501 Not Implemented";
        case BAD_GATEWAY:
            return "502 Bad Gateway";
        case SERVICE_UNAVAILABLE:
            return "503 Service Unavailable";
        default:
            return std::to_string(code);
        }
    }

} // namespace vix::vhttp

#endif // VIX_STATUS_HPP
