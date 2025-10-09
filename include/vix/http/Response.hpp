#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

/**
 * @file Response.hpp
 * @brief Unified HTTP response builder for Vix.cpp applications.
 *
 * @details
 * The `Vix::Response` class provides a **consistent, easy-to-use API**
 * for building and formatting HTTP responses in a Vix.cpp application.
 * It standardizes headers, encodings, and body serialization for JSON
 * and text responses while ensuring full RFC-compliant output via
 * Boost.Beast.
 *
 * ### Core Features
 * - Automatic `Date` and `Server` headers (`Vix` by default)
 * - Thread-safe UTC timestamp formatting
 * - Unified helpers for **error**, **success**, **redirect**, and **no-content**
 *   responses
 * - Generic JSON serialization supporting both `nlohmann::json` and
 *   optional `Vix::json::Json`
 * - Type-safe templated `json_response()` for any structure supporting `.dump()`
 *
 * ### Example
 * ```cpp
 * http::response<http::string_body> res;
 * Vix::Response::json_response(res, nlohmann::json{{"ok", true}, {"version", "1.0"}});
 *
 * // or simple text
 * Vix::Response::text_response(res, "pong", http::status::ok);
 * ```
 */

#include <string>
#include <string_view>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <type_traits>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#if __has_include(<vix/json/json.hpp>)
#include <vix/json/json.hpp>
#define VIX_CORE_HAS_VIX_JSON 1
#else
#define VIX_CORE_HAS_VIX_JSON 0
#endif

namespace Vix
{
    namespace http = boost::beast::http;

    // ------------------------------------------------------------------
    // Helper: to_json_string
    // ------------------------------------------------------------------
    /**
     * @brief Serialize any supported JSON-like object to a string.
     *
     * @tparam J Either `nlohmann::json`, `Vix::json::Json`, or a type exposing `.dump()`.
     * @param j  The JSON object to serialize.
     * @return A UTF-8 encoded JSON string.
     */
    template <class J>
    inline std::string to_json_string(const J &j)
    {
#if VIX_CORE_HAS_VIX_JSON
        if constexpr (std::is_same_v<J, Vix::json::Json>)
        {
            return Vix::json::dumps(j);
        }
        else
#endif
        {
            if constexpr (requires { j.dump(); })
            {
                return j.dump();
            }
            else
            {
                static_assert(sizeof(J) == 0,
                              "Unsupported JSON type: provide nlohmann::json or Vix::json::Json");
            }
        }
    }

    // ------------------------------------------------------------------
    // Helper: http_date_now
    // ------------------------------------------------------------------
    /**
     * @brief Returns the current UTC time formatted for HTTP Date headers.
     * @return A string like `"Wed, 09 Oct 2025 16:32:10 GMT"`.
     *
     * Thread-safe on all major platforms.
     */
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
        tm = *std::gmtime(&t); // fallback (non thread-safe)
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        return oss.str();
    }

    // ------------------------------------------------------------------
    // Response class
    // ------------------------------------------------------------------
    /**
     * @class Response
     * @brief Static utility class for building standardized HTTP responses.
     *
     * Provides convenience methods for JSON and text serialization, standard
     * headers, redirects, and various status shortcuts.
     */
    class Response
    {
    public:
        // --------------------------------------------------------------
        // Common Headers
        // --------------------------------------------------------------
        /**
         * @brief Apply shared headers (`Server`, `Date`) to a response.
         * @param res The response object.
         */
        static void common_headers(http::response<http::string_body> &res) noexcept
        {
            res.set(http::field::server, "Vix");
            res.set(http::field::date, http_date_now());
        }

        // --------------------------------------------------------------
        // Generic creator
        // --------------------------------------------------------------
        /**
         * @brief Build a JSON message response with a given status.
         *
         * @param res          Output response.
         * @param status       HTTP status code.
         * @param message      Message body (converted to JSON with key `message`).
         * @param content_type Content type (defaults to `application/json`).
         */
        static void create_response(http::response<http::string_body> &res,
                                    http::status status,
                                    std::string_view message,
                                    std::string_view content_type = "application/json")
        {
            res.result(status);
            res.set(http::field::content_type, std::string(content_type));
            res.body() = nlohmann::json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        // --------------------------------------------------------------
        // Standard responses
        // --------------------------------------------------------------
        /** @brief Build a standard JSON error response. */
        static void error_response(http::response<http::string_body> &res,
                                   http::status status,
                                   std::string_view message)
        {
            create_response(res, status, message);
        }

        /** @brief Build a standard JSON success (`200 OK`) response. */
        static void success_response(http::response<http::string_body> &res,
                                     std::string_view message)
        {
            create_response(res, http::status::ok, message);
        }

        /** @brief Build a `204 No Content` JSON response. */
        static void no_content_response(http::response<http::string_body> &res,
                                        std::string_view message = "No Content")
        {
            res.result(http::status::no_content);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        /** @brief Build a `302 Found` redirect response with JSON body. */
        static void redirect_response(http::response<http::string_body> &res,
                                      std::string_view location)
        {
            res.result(http::status::found);
            res.set(http::field::location, std::string(location));
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message",
                                         std::string("Redirecting to ") + std::string(location)}}
                             .dump();
            common_headers(res);
            res.prepare_payload();
        }

        // --------------------------------------------------------------
        // JSON responses
        // --------------------------------------------------------------
        /**
         * @brief Generic JSON response builder.
         *
         * @tparam J A type convertible via `to_json_string()` (e.g. `nlohmann::json`).
         * @param res    Response to populate.
         * @param data   Data object to serialize as JSON.
         * @param status HTTP status code (default: `200 OK`).
         */
        template <class J>
        static void json_response(http::response<http::string_body> &res,
                                  const J &data,
                                  http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, "application/json");
            res.body() = to_json_string(data);
            common_headers(res);
            res.prepare_payload();
        }

        // --------------------------------------------------------------
        // Text responses
        // --------------------------------------------------------------
        /**
         * @brief Plain text response builder.
         *
         * @param res    Response to populate.
         * @param data   Text body.
         * @param status HTTP status code (default: `200 OK`).
         */
        static void text_response(http::response<http::string_body> &res,
                                  std::string_view data,
                                  http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, "text/plain");
            res.body() = std::string(data);
            common_headers(res);
            res.prepare_payload();
        }
    };

} // namespace Vix

#endif // VIX_RESPONSE_HPP
