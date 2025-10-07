#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include <vix/http/Response.hpp>
#include <vix/router/RequestHandler.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace Vix
{
    namespace http = boost::beast::http;

    struct RouteNode
    {
        std::unordered_map<std::string, std::unique_ptr<RouteNode>> children;
        std::shared_ptr<IRequestHandler> handler;
        bool isParam = false;
        std::string paramName;
    };

    class Router
    {
    public:
        Router() : root_(std::make_unique<RouteNode>()) {}

        void add_route(http::verb method, const std::string &path, std::shared_ptr<IRequestHandler> handler)
        {
            std::string full_path = method_to_string(method) + path;
            auto *node = root_.get();
            size_t start = 0;

            while (start < full_path.size())
            {
                size_t end = full_path.find('/', start);
                if (end == std::string::npos)
                    end = full_path.size();
                std::string segment = full_path.substr(start, end - start);

                bool isParam = !segment.empty() && segment.front() == '{' && segment.back() == '}';
                std::string key = isParam ? "*" : segment;

                if (!node->children.count(key))
                {
                    node->children[key] = std::make_unique<RouteNode>();
                    node->children[key]->isParam = isParam;
                    if (isParam)
                        node->children[key]->paramName = segment.substr(1, segment.size() - 2);
                }

                node = node->children[key].get();
                start = end + 1;
            }

            node->handler = handler;
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

            std::string full_path = method_to_string(req.method()) + std::string(req.target());
            std::unordered_map<std::string, std::string> params;

            auto *node = root_.get();
            size_t start = 0;

            while (start <= full_path.size() && node)
            {
                if (start == full_path.size())
                    break;
                size_t end = full_path.find('/', start);
                if (end == std::string::npos)
                    end = full_path.size();
                std::string segment = full_path.substr(start, end - start);

                if (node->children.count(segment))
                {
                    node = node->children.at(segment).get();
                }
                else if (node->children.count("*"))
                {
                    node = node->children.at("*").get();
                    params[node->paramName] = segment;
                }
                else
                {
                    node = nullptr;
                    break;
                }

                start = end + 1;
            }

            if (node && node->handler)
            {
                auto *rh = dynamic_cast<RequestHandler<std::function<void(const http::request<http::string_body> &, ResponseWrapper &, std::unordered_map<std::string, std::string> &)>> *>(node->handler.get());
                if (rh)
                {
                    rh->handle_request(req, res);
                }
                else
                {
                    node->handler->handle_request(req, res);
                }
                return true;
            }

            res.result(http::status::not_found);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"message":"Route not found"})";
            return false;
        }

    private:
        std::unique_ptr<RouteNode> root_;

        std::string method_to_string(http::verb method) const
        {
            switch (method)
            {
            case http::verb::get:
                return "GET";
            case http::verb::post:
                return "POST";
            case http::verb::put:
                return "PUT";
            case http::verb::delete_:
                return "DELETE";
            default:
                return "OTHER";
            }
        }
    };

} // namespace Vix

#endif // VIX_ROUTER_HPP
