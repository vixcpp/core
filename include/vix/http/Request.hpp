/**
 *
 *  @file Request.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#ifndef VIX_REQUEST_HPP
#define VIX_REQUEST_HPP

#include <utility>
#include <cassert>
#include <string>
#include <string_view>
#include <memory>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/Simple.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/String.hpp>
#include <vix/http/RequestState.hpp>

namespace vix::vhttp
{
  class Request
  {
  public:
    using RawRequest = http::request<http::string_body>;
    using ParamMap = std::unordered_map<std::string, std::string>;
    using QueryMap = std::unordered_map<std::string, std::string>;
    using StatePtr = std::shared_ptr<vix::vhttp::RequestState>;

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

    Request(const RawRequest &raw,
            ParamMap params)
        : Request(raw, std::move(params), std::make_shared<vix::vhttp::RequestState>())
    {
    }

    Request(const Request &) = default;
    Request &operator=(const Request &) = default;
    Request(Request &&) noexcept = default;
    Request &operator=(Request &&) noexcept = default;
    ~Request() = default;

    const std::string &method() const noexcept { return method_; }
    const std::string &path() const noexcept { return path_; }

    std::string target() const
    {
      return std::string(raw_->target().data(), raw_->target().size());
    }

    const RawRequest &raw() const noexcept { return *raw_; }

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

    std::string param(
        std::string_view key,
        std::string_view fallback = {}) const
    {
      const auto &p = params();
      auto it = p.find(std::string(key));
      return it == p.end() ? std::string(fallback) : it->second;
    }

    const QueryMap &query()
    {
      ensure_query_cache();
      return *query_cache_;
    }

    const QueryMap &query() const
    {
      ensure_query_cache();
      return *query_cache_;
    }

    bool has_query(std::string_view key) const
    {
      ensure_query_cache();
      return query_cache_->find(std::string(key)) != query_cache_->end();
    }

    std::string query_value(std::string_view key, std::string_view fallback = {}) const
    {
      ensure_query_cache();
      auto it = query_cache_->find(std::string(key));
      return it == query_cache_->end() ? std::string(fallback) : it->second;
    }

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

    StatePtr state_ptr() noexcept { return state_; }
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
}

#endif
