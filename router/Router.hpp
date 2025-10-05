#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include "RequestHandler.hpp"
#include "../http/Response.hpp"
#include <boost/beast/http.hpp>
#include <boost/regex.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

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

            std::string path = std::string(req.target());

            for (auto &[key, handler] : routes_)
            {
                if (key.first != req.method())
                    continue;

                auto params = Vix::extract_params_from_path(key.second, path);
                bool is_match = (!params.empty() || key.second == path);

                if (is_match)
                {
                    handler->handle_request(req, res);
                    return true;
                }
            }

            res.result(http::status::not_found);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"message":"Route not found"})";
            return false;
        }

    private:
        std::unordered_map<RouteKey, std::shared_ptr<IRequestHandler>, PairHash> routes_;
    };

} // namespace Vix

#endif // VIX_ROUTER_HPP
