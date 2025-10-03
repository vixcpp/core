#include "DynamicRequestHandler.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <regex>
#include "http/Response.hpp"

using json = nlohmann::json;

namespace Vix
{
    DynamicRequestHandler::DynamicRequestHandler(
        std::function<void(const std::unordered_map<std::string, std::string> &,
                           http::response<http::string_body> &)>
            handler)
        : params_(), handler_(std::move(handler)) {}

    DynamicRequestHandler::~DynamicRequestHandler() {}

    void DynamicRequestHandler::handle_request(const http::request<http::string_body> &req,
                                               http::response<http::string_body> &res)
    {
        if (req.method() == http::verb::get)
        {
            auto id_it = params_.find("id");
            if (id_it != params_.end())
            {
                spdlog::info("Parameter 'id' found: {}", id_it->second);
            }
            handler_(params_, res);
        }
        else if (req.method() == http::verb::put)
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

            if (params_.find("body") != params_.end())
            {
                handler_({{"body", body}}, res);
            }
            else
            {
                Response::error_response(res, http::status::bad_request, "Missing 'body' parameter.");
            }
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

            if (params_.find("body") != params_.end())
            {
                handler_({{"body", body}}, res);
            }
            else
            {
                Response::error_response(res, http::status::bad_request, "Missing 'body' parameter.");
            }
        }
    }

    void DynamicRequestHandler::set_params(
        const std::unordered_map<std::string, std::string> &params,
        http::response<http::string_body> &res)
    {
        for (const auto &param : params)
        {
            const std::string &key = param.first;
            const std::string &value = param.second;
            if (key == "id")
            {
                if (!std::regex_match(value, std::regex("^[0-9]+$")))
                {
                    Response::error_response(res, http::status::bad_request, "Invalid 'id' parameter. Must be a positive integer.");
                    return;
                }
            }
            else if (key == "slug")
            {
                if (!std::regex_match(value, std::regex("^[a-zA-Z0-9_-]+$")))
                {
                    Response::error_response(res, http::status::bad_request, "Invalid 'slug' parameter. Must be alphanumeric.");
                    return;
                }
            }
        }
        params_ = params;
    }

}
