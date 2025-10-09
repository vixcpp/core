#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include <vix/http/Response.hpp>
#include <vix/router/IRequestHandler.hpp>
#include <vix/router/RequestHandler.hpp>

#include <boost/beast/http.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

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
        using NotFoundHandler = std::function<void(
            const http::request<http::string_body> &,
            http::response<http::string_body> &)>;

        Router() : root_(std::make_unique<RouteNode>()) {}

        // Permet de personnaliser la réponse 404
        void setNotFoundHandler(NotFoundHandler h) { notFound_ = std::move(h); }

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

                const bool isParam = !segment.empty() && segment.front() == '{' && segment.back() == '}';
                const std::string key = isParam ? "*" : segment;

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

            node->handler = std::move(handler);
        }

        // Retourne true si une route a géré la requête (même si 204/404)
        bool handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res)
        {
            // CORS preflight
            if (req.method() == http::verb::options)
            {
                res.result(http::status::no_content);
                res.set(http::field::access_control_allow_origin, "*");
                res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD");
                res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
                res.set(http::field::connection, "close");
                res.prepare_payload(); // Content-Length: 0
                return true;
            }

            std::string full_path = method_to_string(req.method()) + std::string(req.target());

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
                    // NB: les params sont extraits dans RequestHandler<> via le pattern de route
                    // Ici, on n’en a pas besoin.
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
                node->handler->handle_request(req, res);
                // Assure un payload préparé (au cas où un handler “oublie”)
                if (res.need_eof() || res.body().size() || res.find(http::field::content_length) != res.end())
                    res.prepare_payload();
                return true;
            }

            // --- Fallback 404 JSON propre ---
            if (notFound_)
            {
                notFound_(req, res);
                // Le notFound_ peut avoir déjà préparé le payload;
                // on assure quand même le coup.
                res.prepare_payload();
            }
            else
            {
                res.result(http::status::not_found);
                nlohmann::json j{
                    {"error", "Route not found"},
                    {"method", std::string(req.method_string())},
                    {"path", std::string(req.target())}};
                Vix::Response::json_response(res, j, res.result());
                // pour éviter que certains clients attendent indéfiniment :
                res.set(http::field::connection, "close");
                res.prepare_payload(); // -> Content-Length correct
            }
            return true; // on a géré la requête (404)
        }

    private:
        std::unique_ptr<RouteNode> root_;
        NotFoundHandler notFound_{};

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
            case http::verb::patch:
                return "PATCH";
            case http::verb::head:
                return "HEAD";
            case http::verb::options:
                return "OPTIONS";
            case http::verb::trace:
                return "TRACE";
            case http::verb::connect:
                return "CONNECT";
            default:
                return "OTHER";
            }
        }
    };

} // namespace Vix

#endif // VIX_ROUTER_HPP
