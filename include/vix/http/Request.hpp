/**
 * @file Request.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_REQUEST_HPP
#define VIX_REQUEST_HPP

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <vix/http/RequestState.hpp>
#include <vix/utils/String.hpp>

namespace vix::http
{
  /**
   * @brief Lightweight native HTTP request object for Vix.
   *
   * This request type is independent of Boost.Beast and is designed to be
   * produced directly by the Vix async HTTP parser / session layer.
   */
  class Request
  {
  public:
    /** @brief Map of route parameters extracted from path templates. */
    using ParamMap = std::unordered_map<std::string, std::string>;

    /** @brief Map of query string parameters parsed from the request target. */
    using QueryMap = std::unordered_map<std::string, std::string>;

    /** @brief Map of HTTP headers. */
    using HeaderMap = std::unordered_map<std::string, std::string>;

    /** @brief Shared pointer type for request-scoped state storage. */
    using StatePtr = std::shared_ptr<vix::http::RequestState>;

    /**
     * @brief Construct an empty request with a default state container.
     */
    Request()
        : params_(std::make_shared<const ParamMap>()),
          query_cache_(nullptr),
          json_cache_(nullptr),
          state_(std::make_shared<vix::http::RequestState>())
    {
    }

    /**
     * @brief Construct a fully initialized request.
     *
     * @param method HTTP method, usually uppercase (GET, POST, ...).
     * @param target Full request target, e.g. "/users?id=1".
     * @param headers Request headers.
     * @param body Request body.
     * @param params Route parameters.
     * @param state Optional request state container.
     */
    Request(
        std::string method,
        std::string target,
        HeaderMap headers = {},
        std::string body = {},
        ParamMap params = {},
        StatePtr state = std::make_shared<vix::http::RequestState>())
        : method_(std::move(method)),
          target_(std::move(target)),
          body_(std::move(body)),
          headers_(std::move(headers)),
          params_(std::make_shared<const ParamMap>(std::move(params))),
          query_cache_(nullptr),
          json_cache_(nullptr),
          state_(std::move(state))
    {
      split_target();
    }

    /**
     * @brief Construct a request without explicitly providing a state container.
     */
    Request(
        std::string method,
        std::string target,
        HeaderMap headers,
        std::string body,
        ParamMap params)
        : Request(
              std::move(method),
              std::move(target),
              std::move(headers),
              std::move(body),
              std::move(params),
              std::make_shared<vix::http::RequestState>())
    {
    }

    Request(const Request &) = default;
    Request &operator=(const Request &) = default;
    Request(Request &&) noexcept = default;
    Request &operator=(Request &&) noexcept = default;
    ~Request() = default;

    /** @brief Return the HTTP method as a string. */
    const std::string &method() const noexcept { return method_; }

    /** @brief Set the HTTP method. */
    void set_method(std::string method) { method_ = std::move(method); }

    /** @brief Return the request path without query string. */
    const std::string &path() const noexcept { return path_; }

    /** @brief Return the raw query string without '?'. */
    const std::string &query_string() const noexcept { return query_raw_; }

    /** @brief Return the full request target. */
    const std::string &target() const noexcept { return target_; }

    /**
     * @brief Set the full request target and recompute path/query caches.
     */
    void set_target(std::string target)
    {
      target_ = std::move(target);
      split_target();
      query_cache_.reset();
    }

    /** @brief Return the route parameters map. */
    const ParamMap &params() const noexcept
    {
      static const ParamMap empty{};
      return params_ ? *params_ : empty;
    }

    /** @brief Replace the route parameters map. */
    void set_params(ParamMap params)
    {
      params_ = std::make_shared<const ParamMap>(std::move(params));
    }

    /** @brief Return true if a route parameter exists. */
    bool has_param(std::string_view key) const
    {
      const auto &p = params();
      return p.find(std::string(key)) != p.end();
    }

    /** @brief Return a route parameter value or fallback if missing. */
    std::string param(std::string_view key, std::string_view fallback = {}) const
    {
      const auto &p = params();
      auto it = p.find(std::string(key));
      return it == p.end() ? std::string(fallback) : it->second;
    }

    /** @brief Return the parsed query map, computed lazily. */
    const QueryMap &query()
    {
      ensure_query_cache();
      return *query_cache_;
    }

    /** @brief Return the parsed query map, computed lazily. */
    const QueryMap &query() const
    {
      ensure_query_cache();
      return *query_cache_;
    }

    /** @brief Return true if a query parameter exists. */
    bool has_query(std::string_view key) const
    {
      ensure_query_cache();
      return query_cache_->find(std::string(key)) != query_cache_->end();
    }

    /** @brief Return a query parameter value or fallback if missing. */
    std::string query_value(std::string_view key, std::string_view fallback = {}) const
    {
      ensure_query_cache();
      auto it = query_cache_->find(std::string(key));
      return it == query_cache_->end() ? std::string(fallback) : it->second;
    }

    /** @brief Return the request body. */
    const std::string &body() const noexcept { return body_; }

    /** @brief Replace the request body and reset cached JSON. */
    void set_body(std::string body)
    {
      body_ = std::move(body);
      json_cache_.reset();
    }

    /** @brief Return all headers. */
    const HeaderMap &headers() const noexcept { return headers_; }

    /** @brief Replace all headers. */
    void set_headers(HeaderMap headers)
    {
      headers_ = std::move(headers);
    }

    /**
     * @brief Return a header value or an empty string if missing.
     *
     * Header lookup is case-sensitive here. The parser/session layer should
     * normalize header names consistently if case-insensitive behavior is desired.
     */
    std::string header(std::string_view name) const
    {
      auto it = headers_.find(std::string(name));
      return it == headers_.end() ? std::string{} : it->second;
    }

    /** @brief Return true if a header exists. */
    bool has_header(std::string_view name) const
    {
      return headers_.find(std::string(name)) != headers_.end();
    }

    /** @brief Set or replace one header. */
    void set_header(std::string name, std::string value)
    {
      headers_[std::move(name)] = std::move(value);
    }

    /** @brief Remove a header if present. */
    void remove_header(std::string_view name)
    {
      headers_.erase(std::string(name));
    }

    /** @brief Parse and return the body as JSON, computed lazily. */
    const nlohmann::json &json() const
    {
      ensure_json_cache();
      return *json_cache_;
    }

    /** @brief Parse the body as JSON and convert it to type T. */
    template <typename T>
    T json_as() const
    {
      ensure_json_cache();
      return json_cache_->get<T>();
    }

    /** @brief Return true if a RequestState container is attached. */
    bool has_state() const noexcept
    {
      return static_cast<bool>(state_);
    }

    /** @brief Return true if the state contains a value of type T. */
    template <class T>
    bool has_state_type() const noexcept
    {
      return state_ && state_->has<T>();
    }

    /** @brief Get a mutable reference to state value of type T. */
    template <class T>
    T &state()
    {
      if (!state_)
        throw std::runtime_error("RequestState not initialized (internal error)");
      return state_->get<T>();
    }

    /** @brief Get a const reference to state value of type T. */
    template <class T>
    const T &state() const
    {
      if (!state_)
        throw std::runtime_error("RequestState not initialized (internal error)");
      return state_->get<T>();
    }

    /** @brief Try to get a mutable pointer to state value of type T. */
    template <class T>
    T *try_state() noexcept
    {
      return state_ ? state_->try_get<T>() : nullptr;
    }

    /** @brief Try to get a const pointer to state value of type T. */
    template <class T>
    const T *try_state() const noexcept
    {
      return state_ ? state_->try_get<T>() : nullptr;
    }

    /** @brief Construct a state value of type T in-place and return a reference to it. */
    template <class T, class... Args>
    T &emplace_state(Args &&...args)
    {
      if (!state_)
        throw std::runtime_error("RequestState not initialized (internal error)");
      return state_->emplace<T>(std::forward<Args>(args)...);
    }

    /** @brief Store or replace a state value of type T. */
    template <class T>
    void set_state(T value)
    {
      if (!state_)
        throw std::runtime_error("RequestState not initialized (internal error)");
      state_->set<T>(std::move(value));
    }

    /** @brief Return the shared state container pointer. */
    StatePtr state_ptr() noexcept { return state_; }

    /** @brief Return the shared state container pointer as const. */
    std::shared_ptr<const vix::http::RequestState> state_ptr() const noexcept
    {
      return state_;
    }

  private:
    void split_target()
    {
      path_.clear();
      query_raw_.clear();

      const std::string_view target_view{target_};
      const auto qpos = target_view.find('?');

      if (qpos == std::string_view::npos)
      {
        path_ = target_;
      }
      else
      {
        path_.assign(target_view.substr(0, qpos));
        query_raw_.assign(target_view.substr(qpos + 1));
      }
    }

    void ensure_query_cache() const
    {
      if (query_cache_)
        return;

      if (query_raw_.empty())
      {
        query_cache_ = std::make_shared<const QueryMap>();
      }
      else
      {
        query_cache_ = std::make_shared<const QueryMap>(
            vix::utils::parse_query_string(query_raw_));
      }
    }

    void ensure_json_cache() const
    {
      if (json_cache_)
        return;

      if (body_.empty())
      {
        json_cache_ = std::make_shared<const nlohmann::json>(nlohmann::json{});
      }
      else
      {
        json_cache_ = std::make_shared<const nlohmann::json>(
            nlohmann::json::parse(body_, nullptr, true, true));
      }
    }

  private:
    std::string method_;
    std::string target_;
    std::string path_;
    std::string query_raw_;
    std::string body_;
    HeaderMap headers_;
    std::shared_ptr<const ParamMap> params_;
    mutable std::shared_ptr<const QueryMap> query_cache_;
    mutable std::shared_ptr<const nlohmann::json> json_cache_;
    StatePtr state_;
  };

} // namespace vix::http

#endif // VIX_REQUEST_HPP
