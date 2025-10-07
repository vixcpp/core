#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

#include <string>
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
            // Vix::json backend
            return Vix::json::dumps(j);
        }
        else
#endif
        {
            // nlohmann::json backend (ou tout type avec .dump())
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

    class Response
    {
    public:
        static void common_headers(http::response<http::string_body> &res)
        {
            res.set(http::field::server, "Vix");
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&now_time_t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            res.set(http::field::date, oss.str());
        }

        static void create_response(http::response<http::string_body> &res,
                                    http::status status,
                                    const std::string &message,
                                    const std::string &content_type = "application/json")
        {
            res.result(status);
            res.set(http::field::content_type, content_type);
            res.body() = nlohmann::json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        static void error_response(http::response<http::string_body> &res,
                                   http::status status,
                                   const std::string &message)
        {
            create_response(res, status, message);
        }

        static void success_response(http::response<http::string_body> &res,
                                     const std::string &message)
        {
            create_response(res, http::status::ok, message);
        }

        static void no_content_response(http::response<http::string_body> &res,
                                        const std::string &message = "No Content")
        {
            res.result(http::status::no_content);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        static void redirect_response(http::response<http::string_body> &res,
                                      const std::string &location)
        {
            res.result(http::status::found);
            res.set(http::field::location, location);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{{"message", "Redirecting to " + location}}.dump();
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
            res.set(http::field::content_type, "text/plain");
            res.body() = std::string(data);
            common_headers(res);
            res.prepare_payload();
        }
    };

} // namespace Vix

#endif // VIX_RESPONSE_HPP
