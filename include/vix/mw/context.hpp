#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include <vix/http/RequestHandler.hpp>
#include <vix/mw/result.hpp>

namespace vix::mw
{
    class Services final
    {
    public:
        Services() = default;

        template <typename T>
        void provide(std::shared_ptr<T> svc)
        {
            data_[std::type_index(typeid(T))] = std::move(svc);
        }

        template <typename T>
        std::shared_ptr<T> get() const
        {
            auto it = data_.find(std::type_index(typeid(T)));
            if (it == data_.end())
                return {};
            return std::static_pointer_cast<T>(it->second);
        }

        template <typename T>
        bool has() const
        {
            return data_.find(std::type_index(typeid(T))) != data_.end();
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<void>> data_{};
    };

    using Request = vix::vhttp::Request;
    using Response = vix::vhttp::ResponseWrapper;

    class Context final
    {
    public:
        Context(Request &req, Response &res, Services &services) noexcept
            : req_(&req), res_(&res), services_(&services)
        {
        }

        Request &req() noexcept { return *req_; }
        const Request &req() const noexcept { return *req_; }

        Response &res() noexcept { return *res_; }
        const Response &res() const noexcept { return *res_; }

        Services &services() noexcept { return *services_; }
        const Services &services() const noexcept { return *services_; }

        template <class T>
        bool has_state() const noexcept
        {
            return req_->template has_state_type<T>();
        }

        template <class T>
        T &state()
        {
            return req_->template state<T>();
        }

        template <class T>
        const T &state() const
        {
            return req_->template state<T>();
        }

        template <class T>
        T *try_state() noexcept
        {
            return req_->template try_state<T>();
        }

        template <class T>
        const T *try_state() const noexcept
        {
            return req_->template try_state<T>();
        }

        template <class T, class... Args>
        T &emplace_state(Args &&...args)
        {
            return req_->template emplace_state<T>(std::forward<Args>(args)...);
        }

        template <class T>
        void set_state(T value)
        {
            req_->template set_state<T>(std::move(value));
        }

        void send_text(std::string_view text, int status = 200)
        {
            res_->status(status).text(text);
        }

        void send_json(const nlohmann::json &j, int status = 200)
        {
            res_->status(status).json(j);
        }

        void send_error(const Error &err)
        {
            nlohmann::json j;
            j["status"] = err.status;
            j["code"] = err.code;
            j["message"] = err.message;

            if (!err.details.empty())
                j["details"] = err.details;

            res_->status(err.status).json(j);
        }

        void send_error(int status,
                        std::string code,
                        std::string message,
                        std::unordered_map<std::string, std::string> details = {})
        {
            Error e;
            e.status = status;
            e.code = std::move(code);
            e.message = std::move(message);
            e.details = std::move(details);
            send_error(e);
        }

    private:
        Request *req_{nullptr};
        Response *res_{nullptr};
        Services *services_{nullptr};
    };

} // namespace vix::mw
