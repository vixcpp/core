#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

/**
 * @file RequestHandler.hpp
 * @brief Functional adapter between user-defined handlers and the Vix routing system.
 *
 * @details
 * `vix::RequestHandler` enables developers to register simple lambdas or callable
 * objects as route handlers without manually subclassing `IRequestHandler`.
 * It provides:
 * - Automatic **parameter extraction** from route patterns (e.g. `/users/{id}`).
 * - A lightweight **ResponseWrapper** offering Express-like chaining (e.g. `res.status(200).json({...})`).
 * - JSON conversion utilities bridging between `vix::json` tokens and `nlohmann::json`.
 *
 * ### Supported handler signatures
 * ```cpp
 * void handler(const http::request<http::string_body>& req, ResponseWrapper& res);
 * void handler(const http::request<http::string_body>& req, ResponseWrapper& res,
 *              std::unordered_map<std::string, std::string>& params);
 * ```
 *
 * ### Example
 * ```cpp
 * router.add_route(http::verb::get, "/users/{id}",
 *     std::make_shared<RequestHandler>("/users/{id}",
 *         [](const auto& req, vix::ResponseWrapper& res, auto& params){
 *             res.status(200).json({{"id", params["id"]}, {"ok", true}});
 *         }
 *     ));
 * ```
 */

#include <string>
#include <string_view>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <initializer_list>
#include <variant>
#include <vector>
#include <utility>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <vix/json/Simple.hpp>

namespace vix::http
{
    namespace http = boost::beast::http;

    // ------------------------------------------------------------------
    // JSON conversion utilities (vix::json → nlohmann::json)
    // ------------------------------------------------------------------

    inline nlohmann::json token_to_nlohmann(const vix::json::token &t);

    /**
     * @brief Convert vix::json::kvs (flat key/value pairs) to nlohmann::json.
     */
    inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list)
    {
        nlohmann::json obj = nlohmann::json::object();
        const auto &a = list.flat;
        const size_t n = a.size() - (a.size() % 2);

        for (size_t i = 0; i < n; i += 2)
        {
            const auto &k = a[i].v;
            const auto &v = a[i + 1];

            if (!std::holds_alternative<std::string>(k))
                continue;
            const std::string &key = std::get<std::string>(k);

            obj[key] = token_to_nlohmann(v);
        }
        return obj;
    }

    inline nlohmann::json token_to_nlohmann(const vix::json::token &t)
    {
        nlohmann::json j = nullptr;
        std::visit([&](auto &&val)
                   {
            using T = std::decay_t<decltype(val)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                j = nullptr;
            } else if constexpr (std::is_same_v<T, bool> ||
                                 std::is_same_v<T, long long> ||
                                 std::is_same_v<T, double> ||
                                 std::is_same_v<T, std::string>) {
                j = val;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::array_t>>) {
                if (!val) { j = nullptr; return; }
                j = nlohmann::json::array();
                for (const auto& el : val->elems) {
                    j.push_back(token_to_nlohmann(el));
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::kvs>>) {
                if (!val) { j = nullptr; return; }
                j = kvs_to_nlohmann(*val);
            } else {
                j = nullptr;
            } }, t.v);
        return j;
    }

    // ------------------------------------------------------------------
    // Path parameter extraction from route pattern
    // ------------------------------------------------------------------
    /**
     * @brief Extract named parameters from a route pattern and a request path.
     *
     * @param pattern e.g. "/users/{id}/posts/{pid}"
     * @param path    Actual request path (e.g. "/users/42/posts/7")
     * @return A map {"id" → "42", "pid" → "7"}
     */
    inline std::unordered_map<std::string, std::string>
    extract_params_from_path(const std::string &pattern, std::string_view path)
    {
        std::unordered_map<std::string, std::string> params;
        size_t rpos = 0, ppos = 0;
        while (rpos < pattern.size() && ppos < path.size())
        {
            if (pattern[rpos] == '{')
            {
                const size_t end_brace = pattern.find('}', rpos);
                const auto name = pattern.substr(rpos + 1, end_brace - rpos - 1);

                const size_t next_slash = path.find('/', ppos);
                const auto value = (next_slash == std::string_view::npos)
                                       ? path.substr(ppos)
                                       : path.substr(ppos, next_slash - ppos);

                params[name] = std::string(value);
                rpos = end_brace + 1;
                ppos = (next_slash == std::string_view::npos) ? path.size() : next_slash + 1;
            }
            else
            {
                ++rpos;
                ++ppos;
            }
        }
        return params;
    }

    // ------------------------------------------------------------------
    // ResponseWrapper — Express-like response builder
    // ------------------------------------------------------------------
    /**
     * @brief Provides an ergonomic API for building HTTP responses.
     *
     * Example:
     * ```cpp
     * res.status(201).json({{"ok", true}, {"id", 7}});
     * ```
     */
    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r) {}

        ResponseWrapper &status(http::status code) noexcept
        {
            res.result(code);
            return *this;
        }
        ResponseWrapper &status(int code) noexcept
        {
            res.result(static_cast<http::status>(code));
            return *this;
        }

        ResponseWrapper &text(std::string_view data)
        {
            vix::http::Response::text_response(res, data, res.result());
            return *this;
        }

        ResponseWrapper &json(std::initializer_list<vix::json::token> list)
        {
            auto j = kvs_to_nlohmann(vix::json::kvs{list});
            vix::http::Response::json_response(res, j, res.result());
            return *this;
        }

        ResponseWrapper &json(const vix::json::kvs &kv)
        {
            auto j = kvs_to_nlohmann(kv);
            vix::http::Response::json_response(res, j, res.result());
            return *this;
        }

        ResponseWrapper &json(const nlohmann::json &j)
        {
            vix::http::Response::json_response(res, j, res.result());
            return *this;
        }

        template <typename J,
                  typename = std::enable_if_t<!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
                                              !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
                                              !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>>>>
        ResponseWrapper &json(const J &data)
        {
            vix::http::Response::json_response(res, data, res.result());
            return *this;
        }
    };

    // ------------------------------------------------------------------
    // RequestHandler<Handler> — generic adapter
    // ------------------------------------------------------------------
    /**
     * @tparam Handler User-provided callable (lambda, functor, etc.).
     *
     * @brief Bridges user handlers to the internal IRequestHandler interface.
     * Supports both (req, res) and (req, res, params) signatures.
     */
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
                    static_assert(always_false<Handler>::value,
                                  "Unsupported handler signature. Use (req,res) or (req,res,params).");
                }

                const bool keep_alive =
                    (req[http::field::connection] == "keep-alive") ||
                    (req.version() == 11 && req[http::field::connection].empty());

                res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
                res.prepare_payload();
            }
            catch (const std::exception &)
            {
                vix::http::Response::error_response(res, http::status::internal_server_error, "Internal Server Error");
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

} // namespace vix::router

#endif // VIX_REQUEST_HANDLER_HPP