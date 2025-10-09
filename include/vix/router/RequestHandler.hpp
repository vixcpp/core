#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

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

#include <vix/router/IRequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <vix/json/Simple.hpp> // token/kvs/array_t + helpers obj()/array()

namespace Vix
{
    namespace http = boost::beast::http;

    // ------------------------------------------------------------------
    // Conversion helpers (Vix::json -> nlohmann::json)
    // ------------------------------------------------------------------

    inline nlohmann::json token_to_nlohmann(const Vix::json::token &t);

    inline nlohmann::json kvs_to_nlohmann(const Vix::json::kvs &list)
    {
        nlohmann::json obj = nlohmann::json::object();
        const auto &a = list.flat;

        // ignorer proprement le dernier élément si liste impaire
        const size_t n = a.size() - (a.size() % 2);

        for (size_t i = 0; i < n; i += 2)
        {
            const auto &k = a[i].v;
            const auto &v = a[i + 1];

            // clé doit être string
            if (!std::holds_alternative<std::string>(k))
                continue;
            const std::string &key = std::get<std::string>(k);

            obj[key] = token_to_nlohmann(v);
        }
        return obj;
    }

    inline nlohmann::json token_to_nlohmann(const Vix::json::token &t)
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
            } else if constexpr (std::is_same_v<T, std::shared_ptr<Vix::json::array_t>>) {
                if (!val) { j = nullptr; return; }
                j = nlohmann::json::array();
                for (const auto& el : val->elems) {
                    j.push_back(token_to_nlohmann(el));
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<Vix::json::kvs>>) {
                if (!val) { j = nullptr; return; }
                j = kvs_to_nlohmann(*val);
            } else {
                j = nullptr;
            } }, t.v);
        return j;
    }

    // ------------------------------------------------------------------
    // Path params extraction: pattern "/users/{id}/posts/{pid}"
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
    // ResponseWrapper: Express-like chaining
    // ------------------------------------------------------------------
    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r) {}

        // status(http::status) & status(int)
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

        // text/plain (garde le status courant)
        ResponseWrapper &text(std::string_view data)
        {
            Vix::Response::text_response(res, data, res.result());
            return *this;
        }

        // JSON — braced-list plate → préféré par l’overload resolution
        ResponseWrapper &json(std::initializer_list<Vix::json::token> list)
        {
            auto j = kvs_to_nlohmann(Vix::json::kvs{list});
            Vix::Response::json_response(res, j, res.result());
            return *this;
        }

        // JSON — Vix::json::kvs (objets plats ou imbriqués via token)
        ResponseWrapper &json(const Vix::json::kvs &kv)
        {
            auto j = kvs_to_nlohmann(kv);
            Vix::Response::json_response(res, j, res.result());
            return *this;
        }

        // JSON — nlohmann::json direct
        ResponseWrapper &json(const nlohmann::json &j)
        {
            Vix::Response::json_response(res, j, res.result());
            return *this;
        }

        // JSON — template générique (évite les collisions via SFINAE)
        template <
            typename J,
            typename = std::enable_if_t<
                !std::is_same_v<std::decay_t<J>, nlohmann::json> &&
                !std::is_same_v<std::decay_t<J>, Vix::json::kvs> &&
                !std::is_same_v<std::decay_t<J>, std::initializer_list<Vix::json::token>>>>
        ResponseWrapper &json(const J &data)
        {
            Vix::Response::json_response(res, data, res.result());
            return *this;
        }
    };

    // ------------------------------------------------------------------
    // RequestHandler<Handler> : adapte les lambdas utilisateur
    //   Signatures supportées:
    //     (req, res, params)  — avec params {name -> value}
    //     (req, res)
    // ------------------------------------------------------------------
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
