#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

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

    // ---------- Helpers ----------
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

    // Thread-safe Date header builder
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
        // fallback non thread-safe si plateforme exotique
        tm = *std::gmtime(&t);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        return oss.str();
    }

    class Response
    {
    public:
        static void common_headers(http::response<http::string_body> &res) noexcept
        {
            res.set(http::field::server, "Vix");
            res.set(http::field::date, http_date_now());
        }

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

        static void error_response(http::response<http::string_body> &res,
                                   http::status status,
                                   std::string_view message)
        {
            create_response(res, status, message);
        }

        static void success_response(http::response<http::string_body> &res,
                                     std::string_view message)
        {
            create_response(res, http::status::ok, message);
        }

        static void no_content_response(http::response<http::string_body> &res,
                                        std::string_view message = "No Content")
        {
            res.result(http::status::no_content);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        static void redirect_response(http::response<http::string_body> &res,
                                      std::string_view location)
        {
            res.result(http::status::found);
            res.set(http::field::location, std::string(location));
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message", std::string("Redirecting to ") + std::string(location)}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        // ---------- JSON générique ----------
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

        static void text_response(http::response<http::string_body> &res,
                                  std::string_view data,
                                  http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, std::string("text/plain"));
            res.body() = std::string(data);
            common_headers(res);
            res.prepare_payload();
        }
    };

} // namespace Vix

#endif // VIX_RESPONSE_HPP
