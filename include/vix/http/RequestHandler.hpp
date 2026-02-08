/**
 * @file RequestHandler.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_REQUEST_HANDLER_HPP
#define VIX_REQUEST_HANDLER_HPP

#include <concepts>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/RequestState.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/utils/Logger.hpp>
#include <vix/utils/String.hpp>

#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace vix::vhttp
{
  namespace http = boost::beast::http;
  using RawRequest = http::request<http::string_body>;

  /** @brief Return the global Vix logger instance. */
  inline vix::utils::Logger &log()
  {
    return vix::utils::Logger::getInstance();
  }

  /** @brief Extract route params from a path using a pattern like "/posts/{id}" and return an empty map on mismatch. */
  inline std::unordered_map<std::string, std::string>
  extract_params_from_path(const std::string &pattern, std::string_view path)
  {
    auto split_path_views = [](std::string_view s) -> std::vector<std::string_view>
    {
      std::vector<std::string_view> out;
      out.reserve(8);

      std::size_t i = 0;
      while (i < s.size() && s[i] == '/')
        ++i;

      while (i < s.size())
      {
        std::size_t j = i;
        while (j < s.size() && s[j] != '/')
          ++j;

        if (j > i)
          out.emplace_back(s.substr(i, j - i));

        i = j;
        while (i < s.size() && s[i] == '/')
          ++i;
      }

      return out;
    };

    std::unordered_map<std::string, std::string> params;

    const auto pSeg = split_path_views(std::string_view(pattern));
    const auto aSeg = split_path_views(path);

    if (pSeg.size() != aSeg.size())
      return {};

    for (std::size_t i = 0; i < pSeg.size(); ++i)
    {
      const auto p = pSeg[i];
      const auto a = aSeg[i];

      const bool isParam = (p.size() >= 3 && p.front() == '{' && p.back() == '}');

      if (isParam)
      {
        const auto name = p.substr(1, p.size() - 2);
        if (!name.empty())
          params.emplace(std::string(name), std::string(a));
      }
      else
      {
        if (p != a)
          return {};
      }
    }

    return params;
  }

  template <class H, class... Args>
  using invoke_result_t = std::invoke_result_t<H, Args...>;

  template <class T>
  using decay_t = std::decay_t<T>;

  /** @brief True for types usable as HTTP status (integral or boost::beast::http::status). */
  template <class T>
  concept HttpStatusLike =
      std::is_integral_v<T> ||
      std::is_same_v<T, http::status>;

  /** @brief True if ResponseWrapper::send(value) is valid for the given type. */
  template <class T>
  concept ReturnSendable =
      requires(ResponseWrapper &res, T &&v) {
        res.send(std::forward<T>(v));
      };

  /** @brief True for pair<status, payload> where payload can be sent by ResponseWrapper. */
  template <class T>
  concept StatusPayloadPair =
      requires {
        typename decay_t<T>::first_type;
        typename decay_t<T>::second_type;
      } &&
      HttpStatusLike<typename decay_t<T>::first_type> &&
      ReturnSendable<typename decay_t<T>::second_type>;

  /** @brief True for tuple<status, payload> where payload can be sent by ResponseWrapper. */
  template <class T>
  concept StatusPayloadTuple =
      requires {
        requires std::tuple_size_v<decay_t<T>> == 2;
        typename std::tuple_element_t<0, decay_t<T>>;
        typename std::tuple_element_t<1, decay_t<T>>;
      } &&
      HttpStatusLike<std::tuple_element_t<0, decay_t<T>>> &&
      ReturnSendable<std::tuple_element_t<1, decay_t<T>>>;

  /** @brief True for values that can be auto-sent (payload or status+payload). */
  template <class T>
  concept Returnable =
      ReturnSendable<T> ||
      StatusPayloadPair<T> ||
      StatusPayloadTuple<T>;

  template <class H>
  concept HandlerFacadeReqRes =
      std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &>>;

  template <class H>
  concept HandlerFacadeReqResParams =
      std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &, const Request::ParamMap &>>;

  template <class H>
  concept HandlerRawReqRes =
      std::is_void_v<invoke_result_t<H, const RawRequest &, ResponseWrapper &>>;

  template <class H>
  concept HandlerRawReqResParams =
      std::is_void_v<invoke_result_t<H, const RawRequest &, ResponseWrapper &, const Request::ParamMap &>>;

  template <class H>
  concept HandlerFacadeReqResRet =
      (!std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &>>) &&
      Returnable<invoke_result_t<H, Request &, ResponseWrapper &>>;

  template <class H>
  concept HandlerFacadeReqResParamsRet =
      (!std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &, const Request::ParamMap &>>) &&
      Returnable<invoke_result_t<H, Request &, ResponseWrapper &, const Request::ParamMap &>>;

  template <class H>
  concept HandlerRawReqResRet =
      (!std::is_void_v<invoke_result_t<H, const RawRequest &, ResponseWrapper &>>) &&
      Returnable<invoke_result_t<H, const RawRequest &, ResponseWrapper &>>;

  template <class H>
  concept HandlerRawReqResParamsRet =
      (!std::is_void_v<invoke_result_t<H, const RawRequest &, ResponseWrapper &, const Request::ParamMap &>>) &&
      Returnable<invoke_result_t<H, const RawRequest &, ResponseWrapper &, const Request::ParamMap &>>;

  /** @brief True for all supported handler signatures (facade/raw, optional params, void or returnable). */
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

  /** @brief Build a simple HTML dev error page with route, method, and path for local debugging. */
  inline std::string make_dev_error_html(
      const std::string &title,
      const std::string &detail,
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

  /** @brief Adapter that wraps a user handler and exposes a uniform IRequestHandler interface for the router/server. */
  template <ValidHandler Handler>
  class RequestHandler : public IRequestHandler
  {
    static_assert(
        ValidHandler<Handler>,
        "Invalid handler signature. Expected:\n"
        "  void (Request&, ResponseWrapper&)\n"
        "  void (Request&, ResponseWrapper&, const Request::ParamMap&)\n"
        "  void (const RawRequest&, ResponseWrapper&)\n"
        "  void (const RawRequest&, ResponseWrapper&, const Request::ParamMap&)\n"
        "  or a returnable value (payload | pair(status,payload) | tuple(status,payload)).");

  public:
    /** @brief Create a handler adapter for a route pattern and a user handler. */
    RequestHandler(std::string route_pattern, Handler handler)
        : route_pattern_(std::move(route_pattern)), handler_(std::move(handler))
    {
      static_assert(sizeof(Handler) > 0, "Handler type must be complete here.");
    }

    /** @brief Execute the user handler and write the final HTTP response. */
    void handle_request(const http::request<http::string_body> &rawReq,
                        http::response<http::string_body> &res) override
    {
      ResponseWrapper wrapped{res};

      std::string_view target(rawReq.target().data(), rawReq.target().size());
      const auto qpos = target.find('?');
      std::string_view path_only =
          (qpos == std::string_view::npos) ? target : target.substr(0, qpos);

      auto params = extract_params_from_path(route_pattern_, path_only);
      auto state = std::make_shared<vix::vhttp::RequestState>();
      vix::vhttp::Request req(rawReq, std::move(params), std::move(state));

      try
      {
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
        log().log(vix::utils::Logger::Level::ERROR,
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
        log().log(vix::utils::Logger::Level::ERROR,
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
    static void maybe_auto_send(
        ResponseWrapper &wrapped,
        http::response<http::string_body> &res,
        T &&value)
    {
      if (!res.body().empty())
        return;

      using V = std::decay_t<T>;

      if constexpr (StatusPayloadPair<V>)
      {
        auto &&v = std::forward<T>(value);

        if constexpr (std::is_same_v<typename V::first_type, http::status>)
          wrapped.status(v.first);
        else
          wrapped.status(static_cast<int>(v.first));

        if (res.result() == http::status::no_content)
        {
          wrapped.send();
          return;
        }

        wrapped.send(v.second);
      }
      else
      {
        wrapped.send(std::forward<T>(value));
      }
    }
  };

} // namespace vix::vhttp

#endif // VIX_REQUEST_HANDLER_HPP
