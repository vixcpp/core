#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include "RequestHandler.hpp"
#include "../http/Response.hpp"

#include <boost/beast/http.hpp>
#include <boost/regex.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace Vix
{
    namespace http = boost::beast::http;
    using json = nlohmann::json;

    struct PairHash
    {
        template <typename T1, typename T2>
        std::size_t operator()(const std::pair<T1, T2> &p) const
        {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };

    class Router
    {
    public:
        using RouteKey = std::pair<http::verb, std::string>;

        Router() = default;
        ~Router() = default;

        void add_route(http::verb method, const std::string &path, std::shared_ptr<IRequestHandler> handler)
        {
            routes_[{method, path}] = std::move(handler);
        }

        bool handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res)
        {
            if (req.method() == http::verb::options)
            {
                res.result(http::status::no_content);
                res.set(http::field::access_control_allow_origin, "*");
                res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD");
                res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
                return true;
            }

            for (auto &[key, handler] : routes_)
            {
                if (key.first == req.method() && matches_dynamic_route(key.second, std::string(req.target()), handler, res, req))
                {
                    return true;
                }
            }

            res.result(http::status::not_found);
            res.set(http::field::content_type, "application/json");
            res.body() = json{{"message", "Route not found"}}.dump();
            return false;
        }

    private:
        bool matches_dynamic_route(const std::string &route_pattern,
                                   const std::string &path,
                                   std::shared_ptr<IRequestHandler> handler,
                                   http::response<http::string_body> &res,
                                   const http::request<http::string_body> &req)
        {
            std::string regex_pattern = convert_route_to_regex(route_pattern);
            boost::regex re(regex_pattern);
            boost::smatch match;

            if (boost::regex_match(path, match, re))
            {
                std::unordered_map<std::string, std::string> params;
                size_t index = 1;
                for (size_t start = 0; (start = route_pattern.find('{', start)) != std::string::npos;)
                {
                    size_t end = route_pattern.find('}', start);
                    std::string name = route_pattern.substr(start + 1, end - start - 1);
                    if (index < match.size())
                    {
                        params[name] = match[index].str();
                        index++;
                    }
                    start = end + 1;
                }

                handler->handle_request(req, res);
                return true;
            }
            return false;
        }

        static std::string convert_route_to_regex(const std::string &pattern)
        {
            std::string regex = "^";
            // L'ancienne variable 'inside' n'était pas utilisée -> suppression
            for (char c : pattern)
            {
                if (c == '{')
                {
                    regex += "(";
                }
                else if (c == '}')
                {
                    regex += "[^/]+)";
                }
                else
                {
                    regex += (c == '/' ? "\\/" : std::string(1, c));
                }
            }
            regex += "$";
            return regex;
        }

        std::unordered_map<RouteKey, std::shared_ptr<IRequestHandler>, PairHash> routes_;
    };

} // namespace Vix

#endif
