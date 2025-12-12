#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

/**
 * @file RequestHandler.hpp
 * @brief Functional adapter between user-defined handlers and the Vix routing system.
 *
 * @details
 * `vix::vhttp::RequestHandler` enables developers to register simple lambdas or callable
 * objects as route handlers without manually subclassing `IRequestHandler`.
 *
 * It provides:
 * - Automatic **parameter extraction** from route patterns (e.g. `/users/{id}`).
 * - A lightweight **Request** façade with:
 *      - `req.method()`
 *      - `req.path()`
 *      - `req.params()["id"]`
 *      - `req.query()["page"]`
 *      - `req.json()` (parsed body as nlohmann::json)
 * - A lightweight **ResponseWrapper** offering Express-like chaining
 *      - `res.status(200).json({...})`
 *      - `res.ok().json({...})`
 *
 * ### Supported handler signatures (user-facing)
 * ```cpp
 * void handler(Request& req, ResponseWrapper& res);
 * void handler(Request& req, ResponseWrapper& res,
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
#include <optional>

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

    // Façades publiques
    class Request;          // req.method(), req.path(), req.params(), req.query(), req.json()
    struct ResponseWrapper; // res.ok().json(...)

    using RawRequest = http::request<http::string_body>;
    // --------------------------------------------------------------
    // Small helpers
    // --------------------------------------------------------------

    inline void ordered_json_response(http::response<http::string_body> &res,
                                      const json::OrderedJson &j,
                                      http::status status_code = http::status::ok)
    {
        res.result(status_code);
        res.body() = j.dump();
        res.set(http::field::content_type, "application/json");
        res.prepare_payload();
    }

    // --------------------------------------------------------------
    // Forward declarations (used by ResponseWrapper)
    // --------------------------------------------------------------
    inline nlohmann::json token_to_nlohmann(const vix::json::token &t);
    inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list);

    // --------------------------------------------------------------
    // URL-decoding helper for query parameters
    // --------------------------------------------------------------
    inline std::string url_decode(std::string_view in)
    {
        std::string out;
        out.reserve(in.size());

        for (size_t i = 0; i < in.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(in[i]);
            if (c == '+')
            {
                out.push_back(' ');
            }
            else if (c == '%' && i + 2 < in.size())
            {
                auto hex = [](unsigned char ch) -> int
                {
                    if (ch >= '0' && ch <= '9')
                        return ch - '0';
                    if (ch >= 'a' && ch <= 'f')
                        return 10 + (ch - 'a');
                    if (ch >= 'A' && ch <= 'F')
                        return 10 + (ch - 'A');
                    return -1;
                };

                int hi = hex(static_cast<unsigned char>(in[i + 1]));
                int lo = hex(static_cast<unsigned char>(in[i + 2]));
                if (hi >= 0 && lo >= 0)
                {
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                }
                else
                {
                    // Malformed % sequence, keep as-is
                    out.push_back(static_cast<char>(c));
                }
            }
            else
            {
                out.push_back(static_cast<char>(c));
            }
        }

        return out;
    }

    inline std::unordered_map<std::string, std::string>
    parse_query_string(std::string_view qs)
    {
        std::unordered_map<std::string, std::string> out;

        size_t pos = 0;
        while (pos < qs.size())
        {
            size_t amp = qs.find('&', pos);
            if (amp == std::string_view::npos)
                amp = qs.size();

            std::string_view pair = qs.substr(pos, amp - pos);
            if (!pair.empty())
            {
                size_t eq = pair.find('=');
                std::string_view key, val;
                if (eq == std::string_view::npos)
                {
                    key = pair;
                    val = std::string_view{};
                }
                else
                {
                    key = pair.substr(0, eq);
                    val = pair.substr(eq + 1);
                }

                auto key_dec = url_decode(key);
                auto val_dec = url_decode(val);
                if (!key_dec.empty())
                {
                    out[std::move(key_dec)] = std::move(val_dec);
                }
            }

            if (amp == qs.size())
                break;
            pos = amp + 1;
        }

        return out;
    }

    // --------------------------------------------------------------
    // Path parameter extraction
    // --------------------------------------------------------------
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
    // Request façade — user-facing API (req.params, req.query, req.json)
    // ------------------------------------------------------------------
    class Request
    {
    public:
        using RawRequest = http::request<http::string_body>;

        Request(const RawRequest &raw,
                std::unordered_map<std::string, std::string> params)
            : raw_(&raw),
              method_(raw.method_string().data(), raw.method_string().size()),
              path_(),
              query_raw_(),
              params_(std::move(params)),
              query_parsed_(false),
              query_(),
              json_parsed_(false),
              body_json_()
        {
            std::string_view target(raw.target().data(), raw.target().size());
            auto qpos = target.find('?');
            if (qpos == std::string_view::npos)
            {
                path_.assign(target.begin(), target.end());
            }
            else
            {
                path_.assign(target.begin(), target.begin() + static_cast<std::ptrdiff_t>(qpos));
                query_raw_.assign(target.begin() + static_cast<std::ptrdiff_t>(qpos + 1), target.end());
            }
        }

        Request(const Request &) = delete;
        Request &operator=(const Request &) = delete;
        Request(Request &&) noexcept = default;
        Request &operator=(Request &&) noexcept = default;
        ~Request() = default;

        // HTTP method ("GET", "POST", ...)
        const std::string &method() const noexcept { return method_; }

        // Raw target ("/users/1?x=1")
        std::string target() const
        {
            return std::string(raw_->target().data(), raw_->target().size());
        }

        // Path without query ("/users/1")
        const std::string &path() const noexcept { return path_; }

        // Params (from route pattern)
        const std::unordered_map<std::string, std::string> &params() const noexcept
        {
            return params_;
        }

        bool has_param(const std::string &key) const
        {
            return params_.find(key) != params_.end();
        }

        std::string param(const std::string &key,
                          const std::string &fallback = "") const
        {
            auto it = params_.find(key);
            if (it == params_.end())
                return fallback;
            return it->second;
        }

        // Query object (lazy-parsed & cached)
        const std::unordered_map<std::string, std::string> &query()
        {
            ensure_query_parsed();
            return query_;
        }

        bool has_query(const std::string &key)
        {
            ensure_query_parsed();
            return query_.find(key) != query_.end();
        }

        std::string query_value(const std::string &key,
                                const std::string &fallback = "")
        {
            ensure_query_parsed();
            auto it = query_.find(key);
            if (it == query_.end())
                return fallback;
            return it->second;
        }

        // Body as raw string
        const std::string &body() const noexcept
        {
            return raw_->body();
        }

        // Body as JSON (lazy parse, throws on error)
        const nlohmann::json &json() const
        {
            ensure_json_parsed();
            return body_json_;
        }

        // Convenience: parse JSON and cast to a type
        template <typename T>
        T json_as() const
        {
            ensure_json_parsed();
            return body_json_.get<T>();
        }

        // Access to underlying Beast request
        const RawRequest &raw() const noexcept { return *raw_; }

        // Header access
        std::string header(std::string_view name) const
        {
            // Beast uses its own string_view type, so we adapt.
            boost::beast::string_view key{name.data(), name.size()};

            auto it = raw_->find(key);
            if (it == raw_->end())
                return {};
            return std::string(it->value());
        }

        bool has_header(std::string_view name) const
        {
            boost::beast::string_view key{name.data(), name.size()};
            return raw_->find(key) != raw_->end();
        }

    private:
        void ensure_query_parsed()
        {
            if (query_parsed_ || query_raw_.empty())
            {
                query_parsed_ = true; // even if empty
                return;
            }

            query_ = parse_query_string(query_raw_);
            query_parsed_ = true;
        }

        void ensure_json_parsed() const
        {
            if (json_parsed_)
                return;

            if (raw_->body().empty())
            {
                body_json_ = nlohmann::json{};
            }
            else
            {
                body_json_ = nlohmann::json::parse(raw_->body(), nullptr, true, true);
            }

            json_parsed_ = true;
        }

    private:
        const RawRequest *raw_;

        std::string method_;
        std::string path_;
        std::string query_raw_;

        std::unordered_map<std::string, std::string> params_;

        mutable bool query_parsed_{false};
        mutable std::unordered_map<std::string, std::string> query_;

        mutable bool json_parsed_{false};
        mutable nlohmann::json body_json_;
    };

    // ------------------------------------------------------------------
    // ResponseWrapper — Express-like response builder
    // ------------------------------------------------------------------
    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r) {}

        ResponseWrapper &status(http::status code) noexcept
        {
            res.result(code);
            return *this;
        }

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

        ResponseWrapper &sendStatus(int code)
        {
            status(code);
            const auto s = vix::vhttp::status_to_string(static_cast<int>(res.result()));
            vix::vhttp::Response::text_response(res, s, res.result());
            return *this;
        }

        ResponseWrapper &text(std::string_view data)
        {
            vix::vhttp::Response::text_response(res, data, res.result());
            return *this;
        }

        ResponseWrapper &json(std::initializer_list<vix::json::token> list)
        {
            auto j = kvs_to_nlohmann(vix::json::kvs{list});
            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        ResponseWrapper &json_ordered(const json::OrderedJson &j)
        {
            vix::vhttp::ordered_json_response(res, j, res.result());
            return *this;
        }

        ResponseWrapper &json(const vix::json::kvs &kv)
        {
            auto j = kvs_to_nlohmann(kv);
            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        ResponseWrapper &json(const nlohmann::json &j)
        {
            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        template <typename J>
            requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
                     !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
                     !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>>)
        ResponseWrapper &json(const J &data)
        {
            vix::vhttp::Response::json_response(res, data, res.result());
            return *this;
        }

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
    // Concepts – handlers compatibles
    // ------------------------------------------------------------------
    template <class H>
    concept HandlerFacadeReqRes =
        std::is_invocable_r_v<void,
                              H,
                              Request &,
                              ResponseWrapper &>;

    template <class H>
    concept HandlerFacadeReqResParams =
        std::is_invocable_r_v<void,
                              H,
                              Request &,
                              ResponseWrapper &,
                              std::unordered_map<std::string, std::string> &>;

    template <class H>
    concept HandlerRawReqRes =
        std::is_invocable_r_v<void,
                              H,
                              const RawRequest &,
                              ResponseWrapper &>;

    template <class H>
    concept HandlerRawReqResParams =
        std::is_invocable_r_v<void,
                              H,
                              const RawRequest &,
                              ResponseWrapper &,
                              std::unordered_map<std::string, std::string> &>;

    template <class H>
    concept ValidHandler =
        HandlerFacadeReqRes<H> ||
        HandlerFacadeReqResParams<H> ||
        HandlerRawReqRes<H> ||
        HandlerRawReqResParams<H>;

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
    // Dev HTML error helper
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
    // RequestHandler<Handler> — bridge Router ↔ user lambda
    // ------------------------------------------------------------------
    template <ValidHandler Handler>
    class RequestHandler : public IRequestHandler
    {
        static_assert(ValidHandler<Handler>,
                      "Invalid handler signature. Expected:\n"
                      "  void (Request&, ResponseWrapper&)\n"
                      "  or\n"
                      "  void (Request&, ResponseWrapper&, unordered_map<string,string>&)");

    public:
        RequestHandler(std::string route_pattern, Handler handler)
            : route_pattern_(std::move(route_pattern)), handler_(std::move(handler))
        {
            static_assert(sizeof(Handler) > 0, "Handler type must be complete here.");
        }

        void handle_request(const http::request<http::string_body> &rawReq,
                            http::response<http::string_body> &res) override
        {
            ResponseWrapper wrapped{res};

            // 1) Extraire la target brute: "/echo/42?page=3&filter=active"
            std::string_view target(rawReq.target().data(), rawReq.target().size());

            // 2) Séparer path / query → on garde seulement le path pour les params
            const auto qpos = target.find('?');
            std::string_view path_only =
                (qpos == std::string_view::npos)
                    ? target                  // "/echo/42"
                    : target.substr(0, qpos); // "/echo/42" (sans "?page=3&filter=active")

            // 3) Extraire les params à partir du pattern et du path seul
            auto params = extract_params_from_path(route_pattern_, path_only);

            // 4) Construire la façade Request (req.params(), req.query(), req.json())
            //    ⚠️ On passe la raw request complète pour que la query soit toujours visible.
            vix::vhttp::Request req(rawReq, params);

            try
            {
                if constexpr (HandlerFacadeReqResParams<Handler>)
                {
                    // Handler Express-like complet : (Request&, ResponseWrapper&, params)
                    handler_(req, wrapped, params);
                }
                else if constexpr (HandlerFacadeReqRes<Handler>)
                {
                    // Handler Express-like simple : (Request&, ResponseWrapper&)
                    handler_(req, wrapped);
                }
                else if constexpr (HandlerRawReqResParams<Handler>)
                {
                    // Handler bas niveau : (rawReq, ResponseWrapper&, params)
                    handler_(rawReq, wrapped, params);
                }
                else if constexpr (HandlerRawReqRes<Handler>)
                {
                    // Handler bas niveau simple : (rawReq, ResponseWrapper&)
                    handler_(rawReq, wrapped);
                }
                else
                {
                    static_assert(ValidHandler<Handler>, "Unsupported handler signature.");
                }

                const bool keep_alive =
                    (rawReq[http::field::connection] == "keep-alive") ||
                    (rawReq.version() == 11 && rawReq[http::field::connection].empty());

                res.set(http::field::connection, keep_alive ? "keep-alive" : "close");
                res.prepare_payload();
            }
            catch (const std::range_error &e)
            {
                auto &log = vix::utils::Logger::getInstance();
                log.log(vix::utils::Logger::Level::ERROR,
                        "Route '{}' threw range_error: {} (method={}, path={})",
                        route_pattern_, e.what(),
                        std::string(rawReq.method_string()), std::string(rawReq.target()));

#ifndef NDEBUG
                res.result(http::status::internal_server_error);
                res.set(http::field::content_type, "text/html; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
                const auto html = make_dev_error_html(
                    "RangeError", e.what(), route_pattern_,
                    std::string(rawReq.method_string()), std::string(rawReq.target()));
                res.body() = html;
                res.prepare_payload();
#else
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
                        std::string(rawReq.method_string()), std::string(rawReq.target()));

#ifndef NDEBUG
                res.result(http::status::internal_server_error);
                res.set(http::field::content_type, "text/html; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");

                const auto html = make_dev_error_html(
                    "Error", e.what(), route_pattern_,
                    std::string(rawReq.method_string()), std::string(rawReq.target()));
                res.body() = html;
                res.prepare_payload();
#else
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
