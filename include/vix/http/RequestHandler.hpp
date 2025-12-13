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
#include <typeindex>
#include <any>

#include <boost/beast/http.hpp>
#include <boost/beast/core/string.hpp>
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
    // Small helpers
    inline void ordered_json_response(
        http::response<http::string_body> &res,
        const json::OrderedJson &j,
        http::status status_code = http::status::ok)
    {
        res.result(status_code);
        res.body() = j.dump();
        res.set(http::field::content_type, "application/json");
        res.prepare_payload();
    }

    // Forward declarations (used by ResponseWrapper)
    inline nlohmann::json token_to_nlohmann(const vix::json::token &t);
    inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list);

    // URL-decoding helper for query parameters
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

    // Path parameter extraction
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

    class RequestState
    {
    public:
        RequestState() = default;

        RequestState(const RequestState &) = delete;
        RequestState &operator=(const RequestState &) = delete;

        RequestState(RequestState &&) noexcept = default;
        RequestState &operator=(RequestState &&) noexcept = default;

        template <class T, class... Args>
        T &emplace(Args &&...args)
        {
            auto key = std::type_index(typeid(T));
            data_[key] = std::make_any<T>(std::forward<Args>(args)...);
            return std::any_cast<T &>(data_[key]);
        }

        template <class T>
        void set(T value)
        {
            auto key = std::type_index(typeid(T));
            data_[key] = std::make_any<T>(std::move(value));
        }

        template <class T>
        bool has() const noexcept
        {
            return data_.find(std::type_index(typeid(T))) != data_.end();
        }

        template <class T>
        T &get()
        {
            auto it = data_.find(std::type_index(typeid(T)));
            if (it == data_.end())
                throw std::runtime_error("Request state not found for type: " + std::string(typeid(T).name()));
            return std::any_cast<T &>(it->second);
        }

        template <class T>
        const T &get() const
        {
            auto it = data_.find(std::type_index(typeid(T)));
            if (it == data_.end())
                throw std::runtime_error("Request state not found for type: " + std::string(typeid(T).name()));
            return std::any_cast<const T &>(it->second);
        }

        template <class T>
        T *try_get() noexcept
        {
            auto it = data_.find(std::type_index(typeid(T)));
            if (it == data_.end())
                return nullptr;
            return std::any_cast<T>(&it->second);
        }

        template <class T>
        const T *try_get() const noexcept
        {
            auto it = data_.find(std::type_index(typeid(T)));
            if (it == data_.end())
                return nullptr;
            return std::any_cast<T>(&it->second);
        }

    private:
        std::unordered_map<std::type_index, std::any> data_;
    };

    // Request façade — user-facing API (req.params, req.query, req.json)
    class Request
    {
    public:
        using RawRequest = http::request<http::string_body>;
        using ParamMap = std::unordered_map<std::string, std::string>;
        using QueryMap = std::unordered_map<std::string, std::string>;
        using StatePtr = std::shared_ptr<vix::vhttp::RequestState>;

        // Main constructor (inject state)
        Request(const RawRequest &raw,
                ParamMap params,
                StatePtr state)
            : raw_(&raw),
              method_(raw.method_string().data(), raw.method_string().size()),
              path_(),
              query_raw_(),
              params_(std::make_shared<const ParamMap>(std::move(params))),
              query_cache_(nullptr),
              json_cache_(nullptr),
              state_(std::move(state))
        {
            std::string_view target(raw.target().data(), raw.target().size());
            const auto qpos = target.find('?');

            if (qpos == std::string_view::npos)
            {
                path_.assign(target.begin(), target.end());
            }
            else
            {
                path_.assign(target.begin(),
                             target.begin() + static_cast<std::ptrdiff_t>(qpos));
                query_raw_.assign(target.begin() + static_cast<std::ptrdiff_t>(qpos + 1),
                                  target.end());
            }
        }

        // (Optional) Convenience constructor (no state)
        // ⚠️ Only use if you really want a Request without state.
        Request(const RawRequest &raw,
                ParamMap params)
            : Request(raw, std::move(params), std::make_shared<vix::vhttp::RequestState>())
        {
        }

        // Copiable / movable (cheap)
        Request(const Request &) = default;
        Request &operator=(const Request &) = default;
        Request(Request &&) noexcept = default;
        Request &operator=(Request &&) noexcept = default;
        ~Request() = default;

        // --------------------------------------------------
        // Core
        // --------------------------------------------------
        const std::string &method() const noexcept { return method_; }
        const std::string &path() const noexcept { return path_; }

        std::string target() const
        {
            return std::string(raw_->target().data(), raw_->target().size());
        }

        const RawRequest &raw() const noexcept { return *raw_; }

        // --------------------------------------------------
        // Params (shared, const)
        // --------------------------------------------------
        const ParamMap &params() const noexcept
        {
            static const ParamMap empty{};
            return params_ ? *params_ : empty;
        }

        bool has_param(std::string_view key) const
        {
            const auto &p = params();
            return p.find(std::string(key)) != p.end();
        }

        std::string param(std::string_view key,
                          std::string_view fallback = {}) const
        {
            const auto &p = params();
            auto it = p.find(std::string(key));
            return it == p.end() ? std::string(fallback) : it->second;
        }

        // --------------------------------------------------
        // Query (lazy + shared)
        // --------------------------------------------------
        const QueryMap &query()
        {
            ensure_query_cache();
            return *query_cache_;
        }

        bool has_query(std::string_view key)
        {
            ensure_query_cache();
            return query_cache_->find(std::string(key)) != query_cache_->end();
        }

        std::string query_value(std::string_view key,
                                std::string_view fallback = {})
        {
            ensure_query_cache();
            auto it = query_cache_->find(std::string(key));
            return it == query_cache_->end() ? std::string(fallback) : it->second;
        }

        // --------------------------------------------------
        // Body / JSON (lazy + shared)
        // --------------------------------------------------
        const std::string &body() const noexcept
        {
            return raw_->body();
        }

        const nlohmann::json &json() const
        {
            ensure_json_cache();
            return *json_cache_;
        }

        template <typename T>
        T json_as() const
        {
            ensure_json_cache();
            return json_cache_->get<T>();
        }

        // --------------------------------------------------
        // Headers
        // --------------------------------------------------
        std::string header(std::string_view name) const
        {
            boost::beast::string_view key{name.data(), name.size()};
            auto it = raw_->find(key);
            return it == raw_->end() ? std::string{} : std::string(it->value());
        }

        bool has_header(std::string_view name) const
        {
            boost::beast::string_view key{name.data(), name.size()};
            return raw_->find(key) != raw_->end();
        }

        // --------------------------------------------------
        // ✅ Point D: typed request-scoped state (FastAPI-like)
        // --------------------------------------------------
        bool has_state() const noexcept
        {
            return static_cast<bool>(state_);
        }

        template <class T>
        bool has_state_type() const noexcept
        {
            return state_ && state_->has<T>();
        }

        template <class T>
        T &state()
        {
            if (!state_)
                throw std::runtime_error("RequestState not initialized (internal error)");
            return state_->get<T>();
        }

        template <class T>
        const T &state() const
        {
            if (!state_)
                throw std::runtime_error("RequestState not initialized (internal error)");
            return state_->get<T>();
        }

        template <class T>
        T *try_state() noexcept
        {
            return state_ ? state_->try_get<T>() : nullptr;
        }

        template <class T>
        const T *try_state() const noexcept
        {
            return state_ ? state_->try_get<T>() : nullptr;
        }

        template <class T, class... Args>
        T &emplace_state(Args &&...args)
        {
            if (!state_)
                throw std::runtime_error("RequestState not initialized (internal error)");
            return state_->emplace<T>(std::forward<Args>(args)...);
        }

        template <class T>
        void set_state(T value)
        {
            if (!state_)
                throw std::runtime_error("RequestState not initialized (internal error)");
            state_->set<T>(std::move(value));
        }

        // Optional: expose state ptr for internal middleware system
        StatePtr state_ptr() noexcept { return state_; }
        std::shared_ptr<const vix::vhttp::RequestState> state_ptr() const noexcept { return state_; }

    private:
        // Lazy builders (shared_ptr)
        void ensure_query_cache()
        {
            if (query_cache_)
                return;

            if (query_raw_.empty())
            {
                query_cache_ = std::make_shared<const QueryMap>();
            }
            else
            {
                query_cache_ = std::make_shared<const QueryMap>(parse_query_string(query_raw_));
            }
        }

        void ensure_json_cache() const
        {
            if (json_cache_)
                return;

            if (raw_->body().empty())
            {
                json_cache_ = std::make_shared<const nlohmann::json>(nlohmann::json{});
            }
            else
            {
                json_cache_ = std::make_shared<const nlohmann::json>(
                    nlohmann::json::parse(raw_->body(), nullptr, true, true));
            }
        }

    private:
        const RawRequest *raw_{nullptr};

        std::string method_;
        std::string path_;
        std::string query_raw_;

        // Shared immutable state
        std::shared_ptr<const ParamMap> params_;

        // Lazy shared caches
        std::shared_ptr<const QueryMap> query_cache_;
        mutable std::shared_ptr<const nlohmann::json> json_cache_;

        // ✅ Request-scoped typed state (shared between copies)
        StatePtr state_;
    };

    // Example
    // app.get("/a", [](Request &, ResponseWrapper &res)
    //         { res.header("X-Powered-By", "Vix")
    //               .type("text/plain; charset=utf-8")
    //               .send("Hello"); });

    // app.get("/b", [](Request &, ResponseWrapper &res)
    //         { res.type("application/problem+json")
    //               .status(400)
    //               .send(nlohmann::json{{"error", "bad"}}); });
    // ResponseWrapper
    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r)
        {
            if (res.result() == http::status::unknown)
                res.result(http::status::ok);
        }

        // Core helpers
        void ensure_status() noexcept
        {
            if (res.result() == http::status::unknown)
                res.result(http::status::ok);
        }

        bool has_header(http::field f) const
        {
            return res.find(f) != res.end();
        }

        bool has_body() const
        {
            // For string_body, body is a string.
            return !res.body().empty();
        }

        // Default message for common status codes (Express-ish)
        static std::string default_status_message(int code)
        {
            auto s = vix::vhttp::status_to_string(code);
            // status_to_string() returns e.g. "404 Not Found" or "200 OK"
            // For default send, we keep it as-is.
            return s;
        }

        // --------------------------------
        // Status
        // --------------------------------
        ResponseWrapper &status(http::status code) noexcept
        {
            res.result(code);
            return *this;
        }

        ResponseWrapper &set_status(http::status code) noexcept { return status(code); }

        ResponseWrapper &status(int code)
        {
            if (code < 100 || code > 599)
            {
#ifndef NDEBUG
                assert(false && "Invalid HTTP status code [100..599]");
                throw std::range_error(
                    "Invalid HTTP status code: " + std::to_string(code) +
                    ". Status code must be between 100 and 599.");
#else
                res.result(http::status::internal_server_error);
                return *this;
#endif
            }

            res.result(vix::vhttp::to_status(code));
            return *this;
        }

        ResponseWrapper &set_status(int code) { return status(code); }

        template <int Code>
        ResponseWrapper &status_c() noexcept
        {
            static_assert(Code >= 100 && Code <= 599, "HTTP status code must be in [100..599]");
            res.result(static_cast<http::status>(Code));
            return *this;
        }

        template <int Code>
        ResponseWrapper &set_status_c() noexcept { return status_c<Code>(); }

        // Headers + type
        ResponseWrapper &header(std::string_view key, std::string_view value)
        {
            res.set(boost::beast::string_view{key.data(), key.size()},
                    boost::beast::string_view{value.data(), value.size()});
            return *this;
        }

        ResponseWrapper &set(std::string_view key, std::string_view value) { return header(key, value); }

        // Append header
        ResponseWrapper &append(std::string_view key, std::string_view value)
        {
            boost::beast::string_view k{key.data(), key.size()};
            boost::beast::string_view v{value.data(), value.size()};

            auto it = res.find(k);
            if (it == res.end())
            {
                res.insert(k, v);
                return *this;
            }

            // If already present, append with comma (standard for many headers).
            // For Set-Cookie, comma concatenation is not ideal in HTTP,
            // but Beast stores only one field entry per name; this is a practical compromise.
            std::string combined = std::string(it->value());
            if (!combined.empty())
                combined += ", ";
            combined.append(v.data(), v.size());

            res.set(k, combined);
            return *this;
        }

        ResponseWrapper &type(std::string_view mime)
        {
            res.set(http::field::content_type,
                    boost::beast::string_view{mime.data(), mime.size()});
            return *this;
        }

        ResponseWrapper &contentType(std::string_view mime) { return type(mime); }

        ResponseWrapper &redirect(std::string_view url)
        {
            // Default redirect code: 302 Found (like Express)
            return redirect(http::status::found, url);
        }

        ResponseWrapper &redirect(http::status code, std::string_view url)
        {
            status(code);
            header("Location", url);

            // Body is optional; but helpful for debugging / clients
            // Keep content-type if user set it, otherwise text/html.
            if (!has_header(http::field::content_type))
            {
                type("text/html; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
            }

            // Minimal HTML response
            std::string body;
            body.reserve(256);
            body += "<!doctype html><html><head><meta charset=\"utf-8\"></head><body>";
            body += "Redirecting to ";
            body += std::string(url);
            body += "</body></html>";

            vix::vhttp::Response::text_response(res, body, res.result());
            return *this;
        }

        // res.redirect("/login")
        // res.redirect(301, "/new-url")
        ResponseWrapper &redirect(int code, std::string_view url)
        {
            status(code);
            return redirect(res.result(), url);
        }

        // sendStatus
        ResponseWrapper &sendStatus(int code)
        {
            status(code);

            // For 204/304 you should not send a body.
            const int s = static_cast<int>(res.result());
            if (s == 204 || s == 304)
                return send(); // empty

            return send(default_status_message(s));
        }

        // Core payload writers
        ResponseWrapper &text(std::string_view data)
        {
            ensure_status();

            // If status implies no body, force empty
            const int s = static_cast<int>(res.result());
            if (s == 204 || s == 304)
                return send(); // empty

            if (!has_header(http::field::content_type))
            {
                type("text/plain; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
            }

            vix::vhttp::Response::text_response(res, data, res.result());
            return *this;
        }

        ResponseWrapper &json(const nlohmann::json &j)
        {
            ensure_status();

            const int s = static_cast<int>(res.result());
            if (s == 204 || s == 304)
                return send(); // empty

            if (!has_header(http::field::content_type))
            {
                type("application/json; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
            }

            vix::vhttp::Response::json_response(res, j, res.result());
            return *this;
        }

        ResponseWrapper &json(const vix::json::kvs &kv)
        {
            auto j = kvs_to_nlohmann(kv);
            return json(j);
        }

        ResponseWrapper &json(std::initializer_list<vix::json::token> list)
        {
            return json(vix::json::kvs{list});
        }

        ResponseWrapper &json_ordered(const json::OrderedJson &j)
        {
            ensure_status();

            const int s = static_cast<int>(res.result());
            if (s == 204 || s == 304)
                return send(); // empty

            if (!has_header(http::field::content_type))
            {
                type("application/json; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
            }

            vix::vhttp::ordered_json_response(res, j, res.result());
            return *this;
        }

        template <typename J>
            requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
                     !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
                     !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>> &&
                     !std::is_same_v<std::decay_t<J>, json::OrderedJson>)
        ResponseWrapper &json(const J &data)
        {
            ensure_status();

            const int s = static_cast<int>(res.result());
            if (s == 204 || s == 304)
                return send(); // empty

            if (!has_header(http::field::content_type))
            {
                type("application/json; charset=utf-8");
                res.set("X-Content-Type-Options", "nosniff");
            }

            vix::vhttp::Response::json_response(res, data, res.result());
            return *this;
        }

        // send() with no args:
        // - If no body written, optionally auto-generate default message (except 204/304)
        // - Here we choose: if body is empty => send default status message,
        //   unless status is 204/304 where body must be empty.
        ResponseWrapper &send()
        {
            ensure_status();

            const int s = static_cast<int>(res.result());
            if (s == 204 || s == 304)
            {
                // Must not include message body
                res.body().clear();
                // Remove content-type if it was set automatically earlier (optional)
                // We keep user headers intact.
                res.prepare_payload();
                return *this;
            }

            if (!has_body())
            {
                // Default body message
                return text(default_status_message(s));
            }

            // If body already exists, just finalize.
            res.prepare_payload();
            return *this;
        }

        // res.status(204).end();
        // res.end()
        ResponseWrapper &end()
        {
            return send();
        }

        // res.location("/login").status(401).send()
        // res.location("/dashboard").status(302).send()
        ResponseWrapper &location(std::string_view url)
        {
            return header("location", url);
        }

        ResponseWrapper &send(std::string_view data) { return text(data); }
        ResponseWrapper &send(const char *data) { return text(data ? std::string_view{data} : std::string_view{}); }
        ResponseWrapper &send(const std::string &data) { return text(std::string_view{data}); }

        ResponseWrapper &send(const nlohmann::json &j) { return json(j); }
        ResponseWrapper &send(const vix::json::kvs &kv) { return json(kv); }
        ResponseWrapper &send(std::initializer_list<vix::json::token> list) { return json(list); }
        ResponseWrapper &send(const json::OrderedJson &j) { return json_ordered(j); }

        template <typename J>
            requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
                     !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
                     !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>> &&
                     !std::is_same_v<std::decay_t<J>, json::OrderedJson> &&
                     !std::is_convertible_v<J, std::string_view>)
        ResponseWrapper &send(const J &data)
        {
            return json(data);
        }

        // send(status, payload)
        template <typename T>
        ResponseWrapper &send(int statusCode, const T &payload)
        {
            status(statusCode);
            return send(payload);
        }

        template <int Code, typename T>
        ResponseWrapper &send_c(const T &payload)
        {
            status_c<Code>();
            return send(payload);
        }

        // sugar
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

    // Helpers
    template <class H, class... Args>
    using invoke_result_t = std::invoke_result_t<H, Args...>;

    template <class T>
    using decay_t = std::decay_t<T>;

    // HTTP status accepted types
    template <class T>
    concept HttpStatusLike =
        std::is_integral_v<T> ||
        std::is_same_v<T, http::status>;

    // VOID handlers (strict)
    template <class H>
    concept HandlerFacadeReqRes =
        std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &>>;

    template <class H>
    concept HandlerFacadeReqResParams =
        std::is_void_v<
            invoke_result_t<H,
                            Request &,
                            ResponseWrapper &,
                            const Request::ParamMap &>>;

    template <class H>
    concept HandlerRawReqRes =
        std::is_void_v<invoke_result_t<H,
                                       const RawRequest &,
                                       ResponseWrapper &>>;

    template <class H>
    concept HandlerRawReqResParams =
        std::is_void_v<
            invoke_result_t<H,
                            const RawRequest &,
                            ResponseWrapper &,
                            const Request::ParamMap &>>;

    // ReturnSendable
    template <class T>
    concept ReturnSendable =
        requires(ResponseWrapper &res, T &&v) {
            res.send(std::forward<T>(v));
        };

    // pair<status, payload>
    template <class T>
    concept StatusPayloadPair =
        requires {
            typename decay_t<T>::first_type;
            typename decay_t<T>::second_type;
        } &&
        HttpStatusLike<typename decay_t<T>::first_type> &&
        ReturnSendable<typename decay_t<T>::second_type>;

    // tuple<status, payload>
    template <class T>
    concept StatusPayloadTuple =
        requires {
            requires std::tuple_size_v<decay_t<T>> == 2;
            typename std::tuple_element_t<0, decay_t<T>>;
            typename std::tuple_element_t<1, decay_t<T>>;
        } &&
        HttpStatusLike<std::tuple_element_t<0, decay_t<T>>> &&
        ReturnSendable<std::tuple_element_t<1, decay_t<T>>>;

    // Returnable (payload | pair | tuple)
    template <class T>
    concept Returnable =
        ReturnSendable<T> ||
        StatusPayloadPair<T> ||
        StatusPayloadTuple<T>;

    // RETURN handlers
    template <class H>
    concept HandlerFacadeReqResRet =
        (!std::is_void_v<invoke_result_t<H,
                                         Request &,
                                         ResponseWrapper &>>) &&
        Returnable<invoke_result_t<H,
                                   Request &,
                                   ResponseWrapper &>>;

    template <class H>
    concept HandlerFacadeReqResParamsRet =
        (!std::is_void_v<invoke_result_t<H,
                                         Request &,
                                         ResponseWrapper &,
                                         const Request::ParamMap &>>) &&
        Returnable<invoke_result_t<H,
                                   Request &,
                                   ResponseWrapper &,
                                   const Request::ParamMap &>>;

    template <class H>
    concept HandlerRawReqResRet =
        (!std::is_void_v<invoke_result_t<H,
                                         const RawRequest &,
                                         ResponseWrapper &>>) &&
        Returnable<invoke_result_t<H,
                                   const RawRequest &,
                                   ResponseWrapper &>>;

    template <class H>
    concept HandlerRawReqResParamsRet =
        (!std::is_void_v<invoke_result_t<H,
                                         const RawRequest &,
                                         ResponseWrapper &,
                                         const Request::ParamMap &>>) &&
        Returnable<invoke_result_t<H,
                                   const RawRequest &,
                                   ResponseWrapper &,
                                   const Request::ParamMap &>>;

    // Final ValidHandler
    template <class H>
    concept ValidHandler =
        HandlerFacadeReqRes<H> ||
        HandlerFacadeReqResParams<H> ||
        HandlerRawReqRes<H> ||
        HandlerRawReqResParams<H> ||
        HandlerFacadeReqResRet<H> ||
        HandlerFacadeReqResParamsRet<H> ||
        HandlerRawReqResRet<H> ||
        HandlerRawReqResParamsRet<H>;

    // JSON conversion utilities (vix::json → nlohmann::json)
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

    // Dev HTML error helper
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

    // RequestHandler<Handler> — bridge Router ↔ user lambda
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

            // Extraire la target brute
            std::string_view target(rawReq.target().data(), rawReq.target().size());

            // Séparer path / query
            const auto qpos = target.find('?');
            std::string_view path_only =
                (qpos == std::string_view::npos) ? target : target.substr(0, qpos);

            // Extraire params (local)
            auto params = extract_params_from_path(route_pattern_, path_only);

            // RequestState : 1 fois par requête
            auto state = std::make_shared<vix::vhttp::RequestState>();

            // Construire Request (params moved -> stocké en shared_ptr<const ParamMap>)
            vix::vhttp::Request req(rawReq, std::move(params), std::move(state));

            try
            {
                // RETURN handlers en premier (sinon le return est ignoré)
                if constexpr (HandlerFacadeReqResParamsRet<Handler>)
                {
                    auto out = handler_(req, wrapped, req.params());
                    maybe_auto_send(wrapped, res, std::move(out));
                }
                else if constexpr (HandlerFacadeReqResRet<Handler>)
                {
                    auto out = handler_(req, wrapped);
                    maybe_auto_send(wrapped, res, std::move(out));
                }
                else if constexpr (HandlerRawReqResParamsRet<Handler>)
                {
                    auto out = handler_(rawReq, wrapped, req.params());
                    maybe_auto_send(wrapped, res, std::move(out));
                }
                else if constexpr (HandlerRawReqResRet<Handler>)
                {
                    auto out = handler_(rawReq, wrapped);
                    maybe_auto_send(wrapped, res, std::move(out));
                }

                // VOID handlers ensuite
                else if constexpr (HandlerFacadeReqResParams<Handler>)
                {
                    handler_(req, wrapped, req.params());
                }
                else if constexpr (HandlerFacadeReqRes<Handler>)
                {
                    handler_(req, wrapped);
                }
                else if constexpr (HandlerRawReqResParams<Handler>)
                {
                    handler_(rawReq, wrapped, req.params());
                }
                else if constexpr (HandlerRawReqRes<Handler>)
                {
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

        template <class T>
        static void maybe_auto_send(ResponseWrapper &wrapped,
                                    http::response<http::string_body> &res,
                                    T &&value)
        {
            if (!res.body().empty())
                return;

            using V = std::decay_t<T>;

            if constexpr (StatusPayloadPair<V>)
            {
                auto &&v = std::forward<T>(value);

                // 1) set status
                if constexpr (std::is_same_v<typename V::first_type, http::status>)
                    wrapped.status(v.first);
                else
                    wrapped.status(static_cast<int>(v.first));

                // 2) 204 => body MUST be empty (force clean)
                if (res.result() == http::status::no_content)
                {
                    wrapped.send();
                    return;
                }

                // 3) send payload
                wrapped.send(v.second);
            }
            else
            {
                // Simple payload => send(payload)
                wrapped.send(std::forward<T>(value));
            }
        }
    };

} // namespace vix::vhttp

#endif // VIX_REQUEST_HANDLER_HPP
