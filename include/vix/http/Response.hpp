#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

namespace Vix
{

    namespace http = boost::beast::http;
    using json = nlohmann::json;

    class Response
    {
    public:
        // Applique des headers communs (Server + Date)
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
            res.body() = json{{"message", message}}.dump();
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
            res.body() = json{{"message", message}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        static void redirect_response(http::response<http::string_body> &res,
                                      const std::string &location)
        {
            res.result(http::status::found);
            res.set(http::field::location, location);
            res.set(http::field::content_type, "application/json");
            res.body() = json{{"message", "Redirecting to " + location}}.dump();
            common_headers(res);
            res.prepare_payload();
        }

        static void json_response(http::response<http::string_body> &res,
                                  const json &data,
                                  http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, "application/json");
            res.body() = data.dump();
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
