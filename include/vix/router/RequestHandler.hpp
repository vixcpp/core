#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

#include <string>
#include <string_view>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/router/IRequestHandler.hpp>
#include <vix/http/Response.hpp>
#include <vix/json/Simple.hpp>

namespace Vix
{

    namespace http = boost::beast::http;

    inline nlohmann::json kvs_to_nlohmann(const json::kvs &list)
    {
        nlohmann::json obj = nlohmann::json::object();
        const auto &a = list.flat;
        // Si impair, on ignore le dernier (ou throw en debug).
        for (size_t i = 0; i + 1 < a.size(); i += 2)
        {
            const auto &k = a[i].v;
            const auto &v = a[i + 1].v;
            if (!std::holds_alternative<std::string>(k))
                continue;
            const std::string &key = std::get<std::string>(k);

            nlohmann::json jv = nullptr;
            std::visit([&](auto &&val)
                       {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) jv = nullptr;
            else jv = val; }, v);

            obj[key] = std::move(jv);
        }
        return obj;
    }

    inline std::unordered_map<std::string, std::string>
    extract_params_from_path(const std::string &pattern, std::string_view path)
    {
        std::unordered_map<std::string, std::string> params;
        size_t rpos = 0, ppos = 0;
        while (rpos < pattern.size() && ppos < path.size())
        {
            if (pattern[rpos] == '{')
            {
                size_t end_brace = pattern.find('}', rpos);
                auto name = pattern.substr(rpos + 1, end_brace - rpos - 1);
                size_t next_slash = path.find('/', ppos);
                auto value = (next_slash == std::string_view::npos)
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

    struct ResponseWrapper
    {
        http::response<http::string_body> &res;

        explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r) {}

        // Allows chaining: res.status(...).json(...)
        ResponseWrapper &status(http::status code) noexcept
        {
            res.result(code);
            return *this;
        }

        // nlohmann::json
        ResponseWrapper &json(const nlohmann::json &j)
        {
            Vix::Response::json_response(res, j, res.result());
            return *this;
        }

        // Vix::json::Json
        template <typename J,
                  typename = std::enable_if_t<!std::is_same_v<J, nlohmann::json>>>
        ResponseWrapper &json(const J &data)
        {
            Vix::Response::json_response(res, data, res.result());
            return *this;
        }

        // Plain text — keeps the current status
        ResponseWrapper &text(std::string_view data)
        {
            Vix::Response::text_response(res, data, res.result());
            return *this;
        }

        // status numérique (style Express)
        ResponseWrapper &status(int code) noexcept
        {
            res.result(static_cast<http::status>(code));
            return *this;
        }

        // json({ "k","v", ... })
        ResponseWrapper &json(const Vix::json::kvs &kv)
        {
            auto j = kvs_to_nlohmann(kv);
            Vix::Response::json_response(res, j, res.result());
            return *this;
        }
    };

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
                    static_assert(always_false<Handler>::value, "Unsupported handler signature");
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
