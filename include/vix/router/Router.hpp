#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

/**
 * @file Router.hpp
 * @brief Trie-based HTTP router with method-aware paths and pluggable 404 handler.
 *
 * @details
 * `vix::Router` maps HTTP requests to handlers using a compact trie keyed by
 * **method-prefixed path segments**. Each node may represent a literal segment
 * or a **parameter segment** (e.g., `{id}`), allowing routes like:
 *
 * - `GET /users` → list users
 * - `GET /users/{id}` → fetch by id
 * - `POST /users` → create user
 *
 * A default **NotFound** callback may be installed to customize 404 responses
 * (e.g., JSON body, diagnostics, or API version hints). Otherwise, a built-in
 * JSON 404 is returned.
 *
 * ### Path matching model
 * The router builds keys by concatenating the method string and the path, then
 * splitting on `/`. Parameter segments are stored with the wildcard key `*`.
 * Extraction of parameter values is delegated to request handlers
 * (e.g., `RequestHandler<T>`), which can decode token names from the
 * route pattern they were registered with.
 *
 * ### CORS preflight
 * `OPTIONS` requests are answered directly by `handle_request()` with
 * `204 No Content` and permissive CORS headers. Application code does not need
 * to register a handler for preflight.
 *
 * ### Thread-safety
 * Route registration is typically done at startup **before** serving traffic.
 * If you mutate the route table after `HTTPServer::run()`, ensure external
 * synchronization.
 */

#include <vix/http/Response.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/RequestHandler.hpp>

#include <boost/beast/http.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

namespace vix::router
{
    namespace http = boost::beast::http;

    /**
     * @brief Trie node representing a path segment.
     * - `children` maps segment token → next node.
     * - `isParam` marks wildcard (parameter) segments (stored under key `*`).
     * - `paramName` holds the token name (e.g., "id") for handlers that need it.
     */
    struct RouteNode
    {
        std::unordered_map<std::string, std::unique_ptr<RouteNode>> children;
        std::shared_ptr<vix::vhttp::IRequestHandler> handler;
        bool isParam = false;
        std::string paramName;

        RouteNode()
            : children(), handler(nullptr), paramName()
        {
        }
    };

    /**
     * @class Router
     * @brief HTTP router resolving requests to `IRequestHandler` instances.
     */
    class Router
    {
    public:
        /**
         * @brief Signature for the user-provided NotFound handler.
         */
        using NotFoundHandler = std::function<void(
            const http::request<http::string_body> &,
            http::response<http::string_body> &)>;

        /** @brief Construct an empty router with a fresh trie root. */
        Router() : root_(std::make_unique<RouteNode>()) {}

        /**
         * @brief Install a custom 404 callback.
         * @param h Functor that fills `res` (status/body/headers). It should
         *          call `prepare_payload()` or use helpers like `Response::json_response()`.
         */
        void setNotFoundHandler(NotFoundHandler h) { notFound_ = std::move(h); }

        /**
         * @brief Register a route.
         * @param method HTTP method (e.g., `http::verb::get`).
         * @param path   Path pattern (may include `{param}` segments).
         * @param handler Shared pointer to an `IRequestHandler` implementation.
         *
         * @details Splits `method_to_string(method)+path` on `/` and inserts
         * nodes, marking `{param}` segments as wildcard (`*`). Parameter value
         * extraction is left to the handler implementation.
         */
        void add_route(http::verb method,
                       const std::string &path,
                       std::shared_ptr<vix::vhttp::IRequestHandler> handler)
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

        /**
         * @brief Resolve and invoke a handler for the request.
         * @return `true` if a response was produced (including 204/404), otherwise `false`.
         *
         * @details
         * - CORS preflight (`OPTIONS`) is auto-answered with `204` and standard
         *   headers.
         * - If a matching handler is found, it is invoked. The router ensures
         *   `prepare_payload()` is called when needed.
         * - If not found, the custom `notFound_` is invoked if present; else a
         *   default JSON 404 is sent.
         */
        bool handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res)
        {
            // CORS preflight
            if (req.method() == http::verb::options)
            {
                res.result(http::status::no_content);
                res.set(http::field::access_control_allow_origin, "*");
                res.set(http::field::access_control_allow_methods,
                        "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD");
                res.set(http::field::access_control_allow_headers,
                        "Content-Type, Authorization");
                res.set(http::field::connection, "close");
                res.prepare_payload();
                return true;
            }

            // Chemin complet utilisé pour la recherche dans le trie
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
                    // NB: parameter extraction is handled inside RequestHandler<>
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
                // Ensure payload prepared (if handler forgot)
                if (res.need_eof() ||
                    res.body().size() ||
                    res.find(http::field::content_length) != res.end())
                {
                    res.prepare_payload();
                }
                return true;
            }

            // --- Fallback 404 JSON ---
            if (notFound_)
            {
                notFound_(req, res);
                res.prepare_payload();
            }
            else
            {
                res.result(http::status::not_found);
                nlohmann::json j{
                    {"error", "Route not found"},
                    {"method", std::string(req.method_string())},
                    {"path", std::string(req.target())}};
                vix::vhttp::Response::json_response(res, j, res.result());
                res.set(http::field::connection, "close");
                res.prepare_payload();
            }
            return true; // handled (404)
        }

    private:
        std::unique_ptr<RouteNode> root_;
        NotFoundHandler notFound_{};

        /**
         * @brief Convert HTTP verb to uppercase string used in the trie root.
         */
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

} // namespace vix::router

#endif // VIX_ROUTER_HPP
