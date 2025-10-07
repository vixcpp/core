#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

#include <string>
#include <string_view>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include <boost/beast/http.hpp>

#include <vix/router/IRequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <nlohmann/json.hpp>

namespace Vix
{

    namespace http = boost::beast::http;
    using json = nlohmann::json;

    // --------------------------------------------------------------
    // Helper: extraire {params} depuis un pattern de route type "/u/{id}"
    // --------------------------------------------------------------
    inline std::unordered_map<std::string, std::string>
    extract_params_from_path(const std::string &route_pattern, std::string_view path)
    {
        std::unordered_map<std::string, std::string> params;

        size_t rpos = 0, ppos = 0;
        while (rpos < route_pattern.size() && ppos < path.size())
        {
            if (route_pattern[rpos] == '{')
            {
                size_t end_brace = route_pattern.find('}', rpos);
                auto param_name = route_pattern.substr(rpos + 1, end_brace - rpos - 1);

                size_t next_slash = path.find('/', ppos);
                auto param_value = (next_slash == std::string_view::npos)
                                       ? path.substr(ppos)
                                       : path.substr(ppos, next_slash - ppos);

                params[param_name] = std::string(param_value);
                rpos = end_brace + 1;
                ppos = (next_slash == std::string_view::npos) ? path.size() : next_slash + 1;
            }
            else
            {
                rpos++;
                ppos++;
            }
        }

        return params;
    }

    // --------------------------------------------------------------
    // Petit wrapper pratique pour construire des réponses
    // --------------------------------------------------------------
    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        void json_body(const json &data, http::status status = http::status::ok)
        {
            Response::json_response(res, data);
            res.result(status);
        }

        void text(std::string_view data, http::status status = http::status::ok)
        {
            res.result(status);
            res.set(http::field::content_type, "text/plain");
            res.body() = std::string(data);
            res.prepare_payload();
        }

        void status(http::status code)
        {
            res.result(code);
        }
    };

    // --------------------------------------------------------------
    // RequestHandler générique (Handler est callable)
    // Signatures supportées :
    //   handler(req, ResponseWrapper&)
    //   handler(req, ResponseWrapper&, std::unordered_map<std::string,std::string>&)
    // --------------------------------------------------------------
    template <typename Handler>
    class RequestHandler : public IRequestHandler
    {
    public:
        RequestHandler(std::string route_pattern, Handler handler)
            : route_pattern_(std::move(route_pattern)), handler_(std::move(handler)) {}

        void handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res) override
        {
            ResponseWrapper wrapped{res};
            try
            {
                auto params = extract_params_from_path(
                    route_pattern_, std::string_view(req.target().data(), req.target().size()));

                if constexpr (std::is_invocable_v<Handler,
                                                  decltype(req), ResponseWrapper &, std::unordered_map<std::string, std::string> &>)
                {
                    handler_(req, wrapped, params);
                }
                else if constexpr (std::is_invocable_v<Handler,
                                                       decltype(req), ResponseWrapper &>)
                {
                    handler_(req, wrapped);
                }
                else
                {
                    static_assert(always_false<Handler>::value, "Unsupported handler signature");
                }

                // keep-alive
                bool keep_alive = (req[http::field::connection] == "keep-alive") ||
                                  (req.version() == 11 && req[http::field::connection].empty());
                res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
                res.prepare_payload();
            }
            catch (const std::exception &)
            {
                Response::error_response(res, http::status::internal_server_error, "Internal Server Error");
            }
        }

    private:
        std::string route_pattern_;
        Handler handler_;

        template <typename T>
        struct always_false : std::false_type
        {
        };
    };

} // namespace Vix

#endif // VIX_REQUEST_HANDLER_HPP
