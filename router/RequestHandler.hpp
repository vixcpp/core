#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

#include "IRequestHandler.hpp"
#include "../http/Response.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <boost/regex.hpp>
#include <boost/beast/http.hpp>
#include <string>
#include <unordered_map>
#include <type_traits>

namespace Vix
{

    namespace http = boost::beast::http;
    using json = nlohmann::json;

    // -------------------- Fonctions utilitaires --------------------

    inline std::unordered_map<std::string, std::string> extract_params_from_path(
        const std::string &route_pattern,
        const std::string &path)
    {
        std::unordered_map<std::string, std::string> params;

        // Convertir /users/{id} en regex ^/users/([^/]+)$
        std::string regex_pattern = "^";
        bool inside_placeholder = false;

        for (char c : route_pattern)
        {
            if (c == '{')
            {
                inside_placeholder = true;
                regex_pattern += "(";
            }
            else if (c == '}')
            {
                inside_placeholder = false;
                regex_pattern += "[^/]+)";
            }
            else
            {
                if (!inside_placeholder)
                {
                    if (c == '/')
                        regex_pattern += "\\/";
                    else
                        regex_pattern += c;
                }
            }
        }
        regex_pattern += "$";

        boost::regex re(regex_pattern);
        boost::smatch match;
        if (boost::regex_match(path, match, re))
        {
            size_t param_index = 1;
            for (size_t start = 0; (start = route_pattern.find('{', start)) != std::string::npos;)
            {
                size_t end = route_pattern.find('}', start);
                std::string param_name = route_pattern.substr(start + 1, end - start - 1);
                if (param_index < match.size())
                {
                    params[param_name] = match[param_index].str();
                    param_index++;
                }
                start = end + 1;
            }
        }

        return params;
    }

    inline std::string extract_param(
        const http::request<http::string_body> &req,
        const std::string &route_pattern,
        const std::string &param_name)
    {
        auto params = extract_params_from_path(route_pattern, std::string(req.target()));
        auto it = params.find(param_name);
        if (it != params.end())
            return it->second;
        return "";
    }

    // -------------------- Wrapper Response --------------------

    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        void json(const json &data, http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, "application/json");
            res.body() = data.dump();
            res.prepare_payload();
        }

        void text(const std::string &data, http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, "text/plain");
            res.body() = data;
            res.prepare_payload();
        }

        void status(http::status code)
        {
            res.result(code);
        }
    };

    // -------------------- RequestHandler générique --------------------

    template <typename Handler>
    class RequestHandler : public IRequestHandler
    {
    public:
        explicit RequestHandler(Handler handler)
            : handler_(std::move(handler)) {}

        void handle_request(
            const http::request<http::string_body> &req,
            http::response<http::string_body> &res) override
        {
            try
            {
                ResponseWrapper wrapped{res};

                if constexpr (std::is_invocable_v<Handler, decltype(req), ResponseWrapper &>)
                {
                    handler_(req, wrapped);
                }
                else if constexpr (std::is_invocable_v<Handler, decltype(req), decltype(res)>)
                {
                    handler_(req, res);
                }
                else
                {
                    static_assert(always_false<Handler>::value, "Handler signature not supported");
                }

                // Keep-Alive
                bool keep_alive = (req[http::field::connection] == "keep-alive") ||
                                  (req.version() == 11 && req[http::field::connection].empty());

                res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
                res.prepare_payload();
            }
            catch (const std::exception &e)
            {
                spdlog::error("Error in RequestHandler: {}", e.what());
                Response::error_response(res, http::status::internal_server_error, "Internal Server Error");
            }
        }

    private:
        Handler handler_;

        template <typename T>
        struct always_false : std::false_type
        {
        };
    };

}

#endif // VIX_REQUEST_HANDLER_HPP
