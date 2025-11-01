#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

/**
 * @file RequestHandler.hpp
 * @brief Functional adapter between user-defined handlers and the Vix routing system.
 *
 * @details
 * `vix::http::RequestHandler` enables developers to register simple lambdas or callable
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
#include <concepts>  // C++20 concepts
#include <cassert>   // assert in Debug
#include <stdexcept> // std::range_error

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>  // to_status(int), status_to_string(int)
#include <vix/json/Simple.hpp>  // vix::json::token / kvs
#include <vix/utils/Logger.hpp> // vix::utils::Logger

namespace vix::vhttp
{
    namespace http = boost::beast::http;

    // --------------------------------------------------------------
    // Forward declarations (used by ResponseWrapper)
    // --------------------------------------------------------------
    inline nlohmann::json token_to_nlohmann(const vix::json::token &t);
    inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list);

    // ------------------------------------------------------------------
    // ResponseWrapper — Express-like response builder
    // ------------------------------------------------------------------
    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r) {}

        /**
         * @brief Set HTTP status from enum and keep chaining.
         */
        ResponseWrapper &status(http::status code) noexcept
        {
            res.result(code);
            return *this;
        }

        /**
         * @brief Set HTTP status from integer (Express-like).
         *
         * - Debug: assert on invalid range and also throw a range_error to bubble into the global handler.
         * - Release: throw a range_error on invalid range so it is **not** silent; the global handler
         *            will log the error and format a proper error response (HTML in Debug builds of the server,
         *            JSON in Release).
         */
        ResponseWrapper &status(int code)
        {
            if (code < 100 || code > 599)
            {
#ifndef NDEBUG
                assert(false && "Invalid HTTP status code [100..599]");
#endif
                throw std::range_error("Invalid HTTP status code: " + std::to_string(code) +
                                       ". Status code must be between 100 and 599.");
            }
            res.result(static_cast<http::status>(code));
            return *this;
        }

        /**
         * @brief Set status and immediately send a short textual payload
         *        like Express's res.sendStatus().
         */
        ResponseWrapper &sendStatus(int code)
        {
            status(code);
            const auto s = vix::vhttp::status_to_string(static_cast<int>(res.result()));
            vix::vhttp::Response::text_response(res, s, res.result());
            return *this;
        }

        /**
         * @brief Send plain text with current status.
         */
        ResponseWrapper &text(std::string_view data)
        {
            vix::vhttp::Response::text_response(res, data, res.result());
            return *this;
        }

        /**
         * @brief Send JSON from key-value tokens (initializer list).
         */
        ResponseWrapper &json(std::initializer_list<vix::json::token> list)
        {
            auto j = kvs_to_nlohmann(vix::json::kvs{list});
            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        /**
         * @brief Send JSON from key-value sequence.
         */
        ResponseWrapper &json(const vix::json::kvs &kv)
        {
            auto j = kvs_to_nlohmann(kv);
            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        /**
         * @brief Send prebuilt nlohmann::json.
         */
        ResponseWrapper &json(const nlohmann::json &j)
        {
            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        /**
         * @brief Send any serializable type as JSON.
         * @note Compile-time error if J is not serializable by Response::json_response.
         */
        template <typename J>
            requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
                     !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
                     !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>>)
        ResponseWrapper &json(const J &data)
        {
            vix::vhttp::Response::json_response(res, data, res.result());
            return *this;
        }

        // ----------------------------------------------------------
        // Express-like helpers (ergonomic chaining)
        // ----------------------------------------------------------
        ResponseWrapper &ok() { return status(http::status::ok); }
        ResponseWrapper &created() { return status(http::status::created); }
        ResponseWrapper &accepted() { return status(http::status::accepted); }
        ResponseWrapper &no_content() { return status(http::status::no_content); }

        ResponseWrapper &bad_request() { return status(http::status::bad_request); }
        ResponseWrapper &unauthorized() { return status(http::status::unauthorized); }
        ResponseWrapper &forbidden() { return status(http::status::forbidden); }
        ResponseWrapper &not_found() { return status(http::status::not_found); }
        ResponseWrapper &conflict() { return status(http::status::conflict); }

        ResponseWrapper &internal_error() { return status(http::status::internal_server_error); }
        ResponseWrapper &not_implemented() { return status(http::status::not_implemented); }
        ResponseWrapper &bad_gateway() { return status(http::status::bad_gateway); }
        ResponseWrapper &service_unavailable() { return status(http::status::service_unavailable); }
    };

    // ------------------------------------------------------------------
    // Concepts — on exige les signatures avec requête const&
    // (évite les faux négatifs et force une API sûre)
    // ------------------------------------------------------------------
    template <class H>
    concept HandlerReqRes =
        std::is_invocable_r_v<void,
                              H,
                              const http::request<http::string_body> &,
                              vix::vhttp::ResponseWrapper &>;

    template <class H>
    concept HandlerReqResParams =
        std::is_invocable_r_v<void,
                              H,
                              const http::request<http::string_body> &,
                              vix::vhttp::ResponseWrapper &,
                              std::unordered_map<std::string, std::string> &>;

    template <class H>
    concept ValidHandler = HandlerReqRes<H> || HandlerReqResParams<H>;

    // ------------------------------------------------------------------
    // JSON conversion utilities (vix::json → nlohmann::json)
    // ------------------------------------------------------------------

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

    // ------------------------------------------------------------------
    // Path parameter extraction
    // ------------------------------------------------------------------
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
    // Utility: Dev HTML error page (Express-like) for invalid status or handler errors.
    // Only used in Debug builds to aid DX. In Release, we return JSON.
    // ------------------------------------------------------------------
    inline std::string make_dev_error_html(const std::string &title, const std::string &detail,
                                           const std::string &route_pattern,
                                           const std::string &method,
                                           const std::string &path)
    {
        std::string html;
        html.reserve(1024);
        html += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>Error</title></head><body><pre>";
        html += title;
        html += ": ";
        html += detail;
        html += "\nRoute: ";
        html += route_pattern;
        html += "\nMethod: ";
        html += method;
        html += "\nPath: ";
        html += path;
        html += "\n</pre></body></html>";
        return html;
    }

    // ------------------------------------------------------------------
    // RequestHandler<Handler> — generic adapter avec checks compile-time
    // ------------------------------------------------------------------
    template <ValidHandler Handler>
    class RequestHandler : public IRequestHandler
    {
        static_assert(ValidHandler<Handler>,
                      "Invalid handler signature. Expected:\n"
                      "  void (const request<string_body>&, ResponseWrapper&)\n"
                      "  or\n"
                      "  void (const request<string_body>&, ResponseWrapper&, unordered_map<string,string>&)");

    public:
        RequestHandler(std::string route_pattern, Handler handler)
            : route_pattern_(std::move(route_pattern)), handler_(std::move(handler))
        {
            static_assert(sizeof(Handler) > 0, "Handler type must be complete here.");
        }

        void handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res) override
        {
            ResponseWrapper wrapped{res};
            try
            {
                auto params = extract_params_from_path(
                    route_pattern_, std::string_view(req.target().data(), req.target().size()));

                if constexpr (HandlerReqResParams<Handler>)
                {
                    handler_(req, wrapped, params);
                }
                else if constexpr (HandlerReqRes<Handler>)
                {
                    handler_(req, wrapped);
                }
                else
                {
                    static_assert(ValidHandler<Handler>, "Unsupported handler signature.");
                }

                const bool keep_alive =
                    (req[http::field::connection] == "keep-alive") ||
                    (req.version() == 11 && req[http::field::connection].empty());

                res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
                res.prepare_payload();
            }
            catch (const std::range_error &e)
            {
                // Explicit developer error (e.g., invalid status code). Log loudly and format Express-like Debug HTML.
                auto &log = vix::utils::Logger::getInstance();
                log.log(vix::utils::Logger::Level::ERROR,
                        "Route '{}' threw range_error: {} (method={}, path={})",
                        route_pattern_, e.what(),
                        std::string(req.method_string()), std::string(req.target()));

#ifndef NDEBUG
                res.result(http::status::internal_server_error);
                res.set(http::field::content_type, "text/html; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
                const auto html = make_dev_error_html(
                    "RangeError", e.what(), route_pattern_,
                    std::string(req.method_string()), std::string(req.target()));
                res.body() = html;
                res.prepare_payload();
#else
                // In Release, return a compact JSON with a clear developer-facing hint.
                nlohmann::json j{
                    {"error", "Internal Server Error"},
                    {"hint", "Invalid status code passed by handler. See server logs."},
                    {"code", "E_INVALID_STATUS"}};
                vix::vhttp::Response::json_response(res, j, http::status::internal_server_error);
                res.prepare_payload();
#endif
            }
            catch (const std::exception &e)
            {
                auto &log = vix::utils::Logger::getInstance();
                log.log(vix::utils::Logger::Level::ERROR,
                        "Route '{}' threw exception: {} (method={}, path={})",
                        route_pattern_, e.what(),
                        std::string(req.method_string()), std::string(req.target()));

#ifndef NDEBUG
                // Debug: rich HTML like Express
                res.result(http::status::internal_server_error);
                res.set(http::field::content_type, "text/html; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");

                const auto html = make_dev_error_html(
                    "Error", e.what(), route_pattern_,
                    std::string(req.method_string()), std::string(req.target()));
                res.body() = html;
                res.prepare_payload();
#else
                // Release: minimal JSON
                vix::vhttp::Response::error_response(
                    res, http::status::internal_server_error, "Internal Server Error");
#endif
            }
        }

    private:
        std::string route_pattern_;
        Handler handler_;
    };

} // namespace vix::vhttp

#endif // VIX_REQUEST_HANDLER_HPP
