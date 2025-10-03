#ifndef UNIFIEDREQUESTHANDLER_HPP
#define UNIFIEDREQUESTHANDLER_HPP

#include "DynamicRequestHandler.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <regex>
#include "http/Response.hpp"

using json = nlohmann::json;

namespace Vix
{
    class UnifiedRequestHandler : public IRequestHandler
    {
    public:
        UnifiedRequestHandler(
            std::function<void(const http::request<http::string_body> &, http::response<http::string_body> &)> handler)
            : handler_(std::move(handler)) {}

        ~UnifiedRequestHandler() {}

        void handle_request(const http::request<http::string_body> &req, http::response<http::string_body> &res) override
        {
            bool keep_alive = (req[http::field::connection] == "keep-alive") ||
                              (req.version() == 11 && req[http::field::connection].empty());

            if (req.method() == http::verb::get)
            {
                std::unordered_map<std::string, std::string> params = extract_dynamic_params_public(std::string(req.target()));
                auto id_it = params.find("id");
                if (id_it != params.end())
                {
                    spdlog::info("Parameter 'id' found: {}", id_it->second);
                }
                handler_(req, res);
            }
            else
            {
                const std::string &body = req.body();
                if (body.empty())
                {
                    Response::error_response(res, http::status::bad_request, "Empty request body.");
                    return;
                }

                json request_json;
                try
                {
                    request_json = json::parse(body);
                }
                catch (const std::exception &e)
                {
                    Response::error_response(res, http::status::bad_request, "Invalid JSON body.");
                    return;
                }

                std::unordered_map<std::string, std::string> params = extract_dynamic_params_public(std::string(req.target()));
                for (auto it = request_json.begin(); it != request_json.end(); ++it)
                {
                    params[it.key()] = it.value();
                }
                handler_(req, res);
            }

            if (keep_alive)
            {
                res.set(http::field::connection, "keep-alive");
            }
            else
            {
                res.set(http::field::connection, "close");
            }

            res.prepare_payload();
        }

        static std::unordered_map<std::string, std::string> extract_dynamic_params_public(const std::string &target)
        {
            return extract_dynamic_params(target);
        }

    private:
        std::function<void(const http::request<http::string_body> &, http::response<http::string_body> &)> handler_;

        static std::unordered_map<std::string, std::string> extract_dynamic_params(const std::string &target)
        {
            std::unordered_map<std::string, std::string> params;
            std::regex regex(R"(^/update_user/(\d+)$)");
            std::smatch matches;

            if (std::regex_match(target, matches, regex))
            {
                params["id"] = matches[1].str();
            }

            return params;
        }

        bool validate_params(const std::unordered_map<std::string, std::string> &params, http::response<http::string_body> &res)
        {
            spdlog::info("Validating parameters...");

            for (const auto &param : params)
            {
                const std::string &key = param.first;
                const std::string &value = param.second;
                if (key == "id")
                {
                    if (!std::regex_match(value, std::regex("^[0-9]+$")))
                    {
                        Response::error_response(res, http::status::bad_request, "Invalid 'id' parameter. Must be a positive integer.");
                        return false;
                    }
                }
                else if (key == "slug")
                {
                    if (!std::regex_match(value, std::regex("^[a-zA-Z0-9_-]+$")))
                    {
                        Response::error_response(res, http::status::bad_request, "Invalid 'slug' parameter. Must be alphanumeric.");
                        return false;
                    }
                }
            }
            return true;
        }
    };

}

#endif
