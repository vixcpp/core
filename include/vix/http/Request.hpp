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

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/RequestState.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/Simple.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/String.hpp>

namespace vix::vhttp
{
  namespace http = boost::beast::http;

  /** @brief HTTP request facade that exposes method, path, params, query, headers, JSON body, and per-request state. */
  class Request
  {
  public:
    /** @brief Underlying Boost.Beast request type used by Vix. */
    using RawRequest = http::request<http::string_body>;

    /** @brief Map of route parameters extracted from path templates. */
    using ParamMap = std::unordered_map<std::string, std::string>;

    /** @brief Map of query string parameters parsed from the request target. */
    using QueryMap = std::unordered_map<std::string, std::string>;

    /** @brief Shared pointer type for request-scoped state storage. */
    using StatePtr = std::shared_ptr<vix::vhttp::RequestState>;

    /** @brief Create a Request view over a raw request, route params, and an optional state container. */
    Request(const RawRequest &raw, ParamMap params, StatePtr state)
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
        path_.assign(target.begin(), target.begin() + static_cast<std::ptrdiff_t>(qpos));
        query_raw_.assign(target.begin() + static_cast<std::ptrdiff_t>(qpos + 1), target.end());
      }
    }

    /** @brief Create a Request with a default RequestState container. */
    Request(const RawRequest &raw, ParamMap params)
        : Request(raw, std::move(params), std::make_shared<vix::vhttp::RequestState>())
    {
    }

    Request(const Request &) = default;
    Request &operator=(const Request &) = default;
    Request(Request &&) noexcept = default;
    Request &operator=(Request &&) noexcept = default;
    ~Request() = default;

    /** @brief Return the HTTP method as an uppercase string (e.g. "GET"). */
    const std::string &method() const noexcept { return method_; }

    /** @brief Return the request path (without the query string). */
    const std::string &path() const noexcept { return path_; }

    /** @brief Return the full target string (path + query) as provided by the client. */
    std::string target() const
    {
      return std::string(raw_->target().data(), raw_->target().size());
    }

    /** @brief Return the underlying raw Boost.Beast request. */
    const RawRequest &raw() const noexcept { return *raw_; }

    /** @brief Return the route parameters map (empty if none). */
    const ParamMap &params() const noexcept
    {
      static const ParamMap empty{};
      return params_ ? *params_ : empty;
    }

    /** @brief Return true if a route parameter exists. */
    bool has_param(std::string_view key) const
    {
      const auto &p = params();
      return p.find(std::string(key)) != p.end();
    }

    /** @brief Return a route parameter value or a fallback string if missing. */
    std::string param(std::string_view key, std::string_view fallback = {}) const
    {
      const auto &p = params();
      auto it = p.find(std::string(key));
      return it == p.end() ? std::string(fallback) : it->second;
    }

    /** @brief Return the parsed query map (computed lazily on first use). */
    const QueryMap &query()
    {
      ensure_query_cache();
      return *query_cache_;
    }

    /** @brief Return the parsed query map (computed lazily on first use). */
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

    /** @brief Return a query parameter value or a fallback string if missing. */
    std::string query_value(std::string_view key, std::string_view fallback = {}) const
    {
      ensure_query_cache();
      auto it = query_cache_->find(std::string(key));
      return it == query_cache_->end() ? std::string(fallback) : it->second;
    }

    /** @brief Return the raw request body string. */
    const std::string &body() const noexcept
    {
      return raw_->body();
    }

    /** @brief Parse and return the request body as JSON (computed lazily on first use). */
    const nlohmann::json &json() const
    {
      ensure_json_cache();
      return *json_cache_;
    }

    /** @brief Parse the request body as JSON and convert it to type T using nlohmann::json conversions. */
    template <typename T>
    T json_as() const
    {
      ensure_json_cache();
      return json_cache_->get<T>();
    }

    /** @brief Return a request header value or an empty string if missing. */
    std::string header(std::string_view name) const
    {
      boost::beast::string_view key{name.data(), name.size()};
      auto it = raw_->find(key);
      return it == raw_->end() ? std::string{} : std::string(it->value());
    }

    /** @brief Return true if a request header exists. */
    bool has_header(std::string_view name) const
    {
      boost::beast::string_view key{name.data(), name.size()};
      return raw_->find(key) != raw_->end();
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

    /** @brief Get a mutable reference to state value of type T (throws if state is missing). */
    template <class T>
    T &state()
    {
      if (!state_)
        throw std::runtime_error("RequestState not initialized (internal error)");
      return state_->get<T>();
    }

    /** @brief Get a const reference to state value of type T (throws if state is missing). */
    template <class T>
    const T &state() const
    {
      if (!state_)
        throw std::runtime_error("RequestState not initialized (internal error)");
      return state_->get<T>();
    }

    /** @brief Try to get a mutable pointer to state value of type T or nullptr if missing. */
    template <class T>
    T *try_state() noexcept
    {
      return state_ ? state_->try_get<T>() : nullptr;
    }

    /** @brief Try to get a const pointer to state value of type T or nullptr if missing. */
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

    /** @brief Return the shared state container pointer (mutable). */
    StatePtr state_ptr() noexcept { return state_; }

    /** @brief Return the shared state container pointer (const). */
    std::shared_ptr<const vix::vhttp::RequestState> state_ptr() const noexcept { return state_; }

  private:
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
        query_cache_ = std::make_shared<const QueryMap>(vix::utils::parse_query_string(query_raw_));
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
    std::shared_ptr<const ParamMap> params_;
    mutable std::shared_ptr<const QueryMap> query_cache_;
    mutable std::shared_ptr<const nlohmann::json> json_cache_;
    StatePtr state_;
  };

} // namespace vix::vhttp

#endif // VIX_REQUEST_HPP
