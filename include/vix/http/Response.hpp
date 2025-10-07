#ifndef VIX_RESPONSE_HPP
#define VIX_RESPONSE_HPP

#include <string>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>

using json = nlohmann::json;
namespace http = boost::beast::http;
namespace fs = boost::filesystem;

namespace Vix
{
    class Response
    {
    public:
        static void create_response(
            http::response<http::string_body> &res,
            http::status status,
            const std::string &message,
            const std::string &content_type = "application/json")
        {
            res.result(status);
            res.set(http::field::content_type, content_type);
            res.set(http::field::server, "Vix");
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&now_time_t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            std::string date = oss.str();
            res.set(http::field::date, date);
            res.body() = json{{"message", message}}.dump();
        }

        static void error_response(
            http::response<http::string_body> &res,
            http::status status,
            const std::string &message)
        {
            create_response(res, status, message);
        }

        static void success_response(
            http::response<http::string_body> &res,
            const std::string &message)
        {
            create_response(res, http::status::ok, message);
        }

        static void no_content_response(
            http::response<http::string_body> &res,
            const std::string &message = "No Content")
        {
            res.result(http::status::no_content);
            res.set(http::field::content_type, "application/json");
            res.body() = json{{"message", message}}.dump();
            res.set(http::field::server, "Vix");
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&now_time_t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            std::string date = oss.str();
            res.set(http::field::date, date);
        }

        static void redirect_response(http::response<http::string_body> &res,
                                      const std::string &location)
        {
            res.result(http::status::found);
            res.set(http::field::location, location);
            res.set(http::field::content_type, "application/json");
            res.body() = json{{"message", "Redirecting to " + location}}.dump();
            res.set(http::field::server, "Vix");
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&now_time_t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            std::string date = oss.str();
            res.set(http::field::date, date);
        }

        static void json_response(http::response<http::string_body> &res,
                                  const json &data)
        {
            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = data.dump();
            res.set(http::field::server, "Vix");
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::gmtime(&now_time_t);

            std::ostringstream oss;
            oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            std::string date = oss.str();
            res.set(http::field::date, date);
        }
    };
}

#endif