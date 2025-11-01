#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

/**
 * @file Response.hpp
 * @brief Unified HTTP response builder for Vix.cpp applications.
 *
 * @details
 * The `vix::Response` class provides a **consistent, easy-to-use API**
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
 *   optional `vix::json::Json`
 * - Type-safe templated `json_response()` for any structure supporting `.dump()`
 *
 * ### Example
 * ```cpp
 * boost::beast::http::response<boost::beast::http::string_body> res;
 * vix::http::Response::json_response(res, nlohmann::json{{"ok", true}, {"version", "1.0"}});
 * vix::http::Response::text_response(res, "pong", vix::http::to_status(200));
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

#include <vix/http/Status.hpp> // to_status(int) + (optionally) named int constants

#if __has_include(<vix/json/json.hpp>)
#include <vix/json/json.hpp>
#define VIX_CORE_HAS_VIX_JSON 1
#else
#define VIX_CORE_HAS_VIX_JSON 0
#endif

namespace vix::vhttp
{

    namespace http = boost::beast::http;
    // --------------------------------------------------------------
    // Concepts / traits (compile-time checks)
    // --------------------------------------------------------------
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
    // Fallback SFINAE (sans concepts)
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

    // --------------------------------------------------------------
    // Helper: to_json_string (compile-time validated)
    // --------------------------------------------------------------
    /**
     * @brief Serialize any supported JSON-like object to a string.
     *
     * @tparam J Either `nlohmann::json`, `vix::json::Json`, or a type exposing `.dump()`.
     * @param j  The JSON object to serialize.
     * @return A UTF-8 encoded JSON string.
     */
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
                // nlohmann::json path (has dump() also, but explicit branch keeps intent clear)
                return j.dump();
            }
        }
    }

    // --------------------------------------------------------------
    // Helper: http_date_now
    // --------------------------------------------------------------
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

    // --------------------------------------------------------------
    // Compile-time validator for status codes
    // --------------------------------------------------------------
    template <int Code>
    struct status_code_in_range
    {
        static constexpr bool value = (Code >= 100) && (Code <= 599);
    };

    // --------------------------------------------------------------
    // Response class
    // --------------------------------------------------------------
    class Response
    {
    public:
        // Common headers
        static void common_headers(http::response<http::string_body> &res) noexcept
        {
            res.set(http::field::server, "Vix/master");
            res.set(http::field::date, http_date_now());
        }

        // ----------------------------------------------------------
        // create_response (enum)
        // ----------------------------------------------------------
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

        // create_response (int) — runtime
        static void create_response(http::response<http::string_body> &res,
                                    int status,
                                    std::string_view message,
                                    std::string_view content_type = "application/json")
        {
            create_response(res, to_status(status), message, content_type);
        }

        // create_response<Code> — compile-time error if Code ∉ [100..599]
        template <int Code>
        static void create_response(http::response<http::string_body> &res,
                                    std::string_view message,
                                    std::string_view content_type = "application/json")
        {
            static_assert(status_code_in_range<Code>::value,
                          "HTTP status code must be between 100 and 599");
            create_response(res, static_cast<http::status>(Code), message, content_type);
        }

        // ----------------------------------------------------------
        // error_response
        // ----------------------------------------------------------
        static void error_response(http::response<http::string_body> &res,
                                   http::status status,
                                   std::string_view message)
        {
            create_response(res, status, message);
        }
        static void error_response(http::response<http::string_body> &res,
                                   int status,
                                   std::string_view message)
        {
            create_response(res, to_status(status), message);
        }
        template <int Code>
        static void error_response(http::response<http::string_body> &res,
                                   std::string_view message)
        {
            static_assert(status_code_in_range<Code>::value,
                          "HTTP status code must be between 100 and 599");
            create_response(res, static_cast<http::status>(Code), message);
        }

        // ----------------------------------------------------------
        // success_response (200 OK)
        // ----------------------------------------------------------
        static void success_response(http::response<http::string_body> &res,
                                     std::string_view message)
        {
            create_response(res, http::status::ok, message);
        }

        // ----------------------------------------------------------
        // no_content_response (204)
        // ----------------------------------------------------------
        static void no_content_response(http::response<http::string_body> &res,
                                        std::string_view message = "No Content")
        {
            res.result(http::status::no_content);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        // ----------------------------------------------------------
        // redirect_response (302)
        // ----------------------------------------------------------
        static void redirect_response(http::response<http::string_body> &res,
                                      std::string_view location)
        {
            res.result(http::status::found);
            res.set(http::field::location, std::string(location));
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{
                {"message", std::string("Redirecting to ") + std::string(location)}}
                             .dump();
            common_headers(res);
            res.prepare_payload();
        }

        // ----------------------------------------------------------
        // json_response (enum)
        // ----------------------------------------------------------
        template <class J>
#if __cpp_concepts
            requires SupportedJson<J>
#endif
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

        // json_response (int) — runtime
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

        // json_response<Code> — compile-time check
        template <int Code, class J>
#if __cpp_concepts
            requires SupportedJson<J>
#endif
        static void json_response(http::response<http::string_body> &res,
                                  const J &data)
        {
            static_assert(status_code_in_range<Code>::value,
                          "HTTP status code must be between 100 and 599");
            json_response(res, data, static_cast<http::status>(Code));
        }

        // ----------------------------------------------------------
        // text_response (enum / int / Code)
        // ----------------------------------------------------------
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

        static void text_response(http::response<http::string_body> &res,
                                  std::string_view data,
                                  int status)
        {
            text_response(res, data, to_status(status));
        }

        template <int Code>
        static void text_response(http::response<http::string_body> &res,
                                  std::string_view data)
        {
            static_assert(status_code_in_range<Code>::value,
                          "HTTP status code must be between 100 and 599");
            text_response(res, data, static_cast<http::status>(Code));
        }
    };

} // namespace vix::vhttp

#endif // VIX_RESPONSE_HPP
