/**
 *
 * @file context.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_CONTEXT_HPP
#define VIX_CONTEXT_HPP

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

  /**
   * @brief Simple service container for middleware and handlers.
   */
  class Services final
  {
  public:
    Services() = default;

    /** @brief Register a service instance by type. */
    template <typename T>
    void provide(std::shared_ptr<T> svc)
    {
      data_[std::type_index(typeid(T))] = std::move(svc);
    }

    /** @brief Get a service by type (empty if not found). */
    template <typename T>
    std::shared_ptr<T> get() const
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        return {};
      return std::static_pointer_cast<T>(it->second);
    }

    /** @brief Check if a service type is registered. */
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

  /**
   * @brief Request/response context passed through middleware pipelines.
   */
  class Context final
  {
  public:
    /** @brief Construct a context with request, response, and services. */
    Context(Request &req, Response &res, Services &services) noexcept
        : req_(&req), res_(&res), services_(&services) {}

    /** @brief Access the request. */
    Request &req() noexcept { return *req_; }

    /** @brief Access the request (const). */
    const Request &req() const noexcept { return *req_; }

    /** @brief Access the response. */
    Response &res() noexcept { return *res_; }

    /** @brief Access the response (const). */
    const Response &res() const noexcept { return *res_; }

    /** @brief Access the service container. */
    Services &services() noexcept { return *services_; }

    /** @brief Access the service container (const). */
    const Services &services() const noexcept { return *services_; }

    /** @brief Check whether a request-scoped state of type T exists. */
    template <class T>
    bool has_state() const noexcept
    {
      return req_->template has_state_type<T>();
    }

    /** @brief Get request-scoped state of type T. */
    template <class T>
    T &state()
    {
      return req_->template state<T>();
    }

    /** @brief Get request-scoped state of type T (const). */
    template <class T>
    const T &state() const
    {
      return req_->template state<T>();
    }

    /** @brief Try to get request-scoped state of type T (nullptr if missing). */
    template <class T>
    T *try_state() noexcept
    {
      return req_->template try_state<T>();
    }

    /** @brief Try to get request-scoped state of type T (nullptr if missing). */
    template <class T>
    const T *try_state() const noexcept
    {
      return req_->template try_state<T>();
    }

    /** @brief Construct request-scoped state of type T in-place and return it. */
    template <class T, class... Args>
    T &emplace_state(Args &&...args)
    {
      return req_->template emplace_state<T>(std::forward<Args>(args)...);
    }

    /** @brief Set/replace request-scoped state of type T. */
    template <class T>
    void set_state(T value)
    {
      req_->template set_state<T>(std::move(value));
    }

    /** @brief Send a plain text response. */
    void send_text(std::string_view text, int status = 200)
    {
      res_->status(status).text(text);
    }

    /** @brief Send a JSON response. */
    void send_json(const nlohmann::json &j, int status = 200)
    {
      res_->status(status).json(j);
    }

    /** @brief Send a standardized error response from an Error object. */
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

    /** @brief Build and send a standardized error response. */
    void send_error(
        int status,
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

#endif // VIX_CONTEXT_HPP
