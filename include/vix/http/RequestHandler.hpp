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
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <vix/async/core/task.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/RequestState.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/http/Status.hpp>
#include <vix/utils/Logger.hpp>

#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace vix::vhttp
{
  using vix::async::core::task;

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

      const bool is_param = (p.size() >= 3 && p.front() == '{' && p.back() == '}');

      if (is_param)
      {
        const auto name = p.substr(1, p.size() - 2);
        if (!name.empty())
          params.emplace(std::string(name), std::string(a));
      }
      else if (p != a)
      {
        return {};
      }
    }

    return params;
  }

  template <class H, class... Args>
  using invoke_result_t = std::invoke_result_t<H, Args...>;

  template <class T>
  using decay_t = std::decay_t<T>;

  /** @brief True for integral HTTP status codes. */
  template <class T>
  concept HttpStatusLike =
      std::is_integral_v<decay_t<T>>;

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
  concept HandlerReqResVoid =
      std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &>>;

  template <class H>
  concept HandlerReqResParamsVoid =
      std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &, const Request::ParamMap &>>;

  template <class H>
  concept HandlerReqResRet =
      (!std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &>>) &&
      Returnable<invoke_result_t<H, Request &, ResponseWrapper &>>;

  template <class H>
  concept HandlerReqResParamsRet =
      (!std::is_void_v<invoke_result_t<H, Request &, ResponseWrapper &, const Request::ParamMap &>>) &&
      Returnable<invoke_result_t<H, Request &, ResponseWrapper &, const Request::ParamMap &>>;

  /** @brief True for all supported native handler signatures. */
  template <class H>
  concept ValidHandler =
      HandlerReqResVoid<H> ||
      HandlerReqResParamsVoid<H> ||
      HandlerReqResRet<H> ||
      HandlerReqResParamsRet<H>;

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

  /** @brief Adapter that wraps a user handler and exposes a uniform native IRequestHandler interface for the router/server. */
  template <ValidHandler Handler>
  class RequestHandler final : public IRequestHandler
  {
    static_assert(
        ValidHandler<Handler>,
        "Invalid handler signature. Expected:\n"
        "  void (Request&, ResponseWrapper&)\n"
        "  void (Request&, ResponseWrapper&, const Request::ParamMap&)\n"
        "  payload (sendable by ResponseWrapper)\n"
        "  pair(status, payload)\n"
        "  tuple(status, payload)");

  public:
    /** @brief Create a handler adapter for a route pattern and a user handler. */
    RequestHandler(std::string route_pattern,
                   Handler handler,
                   vix::view::TemplateView *template_view = nullptr)
        : route_pattern_(std::move(route_pattern)),
          handler_(std::move(handler)),
          template_view_(template_view)
    {
      static_assert(sizeof(Handler) > 0, "Handler type must be complete here.");
    }

    /** @brief Execute the user handler and write the final HTTP response. */
    task<void> handle_request(const Request &incoming_req, Response &res) override
    {
      ResponseWrapper wrapped{res, template_view_};

      auto params = extract_params_from_path(route_pattern_, incoming_req.path());
      auto state = incoming_req.state_ptr()
                       ? std::const_pointer_cast<RequestState>(incoming_req.state_ptr())
                       : std::make_shared<RequestState>();

      Request req(
          incoming_req.method(),
          incoming_req.target(),
          incoming_req.headers(),
          incoming_req.body(),
          std::move(params),
          std::move(state));

      try
      {
        if constexpr (HandlerReqResParamsRet<Handler>)
        {
          auto out = handler_(req, wrapped, req.params());
          maybe_auto_send(wrapped, res, std::move(out));
        }
        else if constexpr (HandlerReqResRet<Handler>)
        {
          auto out = handler_(req, wrapped);
          maybe_auto_send(wrapped, res, std::move(out));
        }
        else if constexpr (HandlerReqResParamsVoid<Handler>)
        {
          handler_(req, wrapped, req.params());
        }
        else if constexpr (HandlerReqResVoid<Handler>)
        {
          handler_(req, wrapped);
        }
        else
        {
          static_assert(ValidHandler<Handler>, "Unsupported handler signature.");
        }

        finalize_response(req, res);
      }
      catch (const std::range_error &e)
      {
        log().log(vix::utils::Logger::Level::Error,
                  "Route '{}' threw range_error: {} (method={}, path={})",
                  route_pattern_, e.what(), req.method(), req.path());

#ifndef NDEBUG
        res.set_status(INTERNAL_ERROR);
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_body(make_dev_error_html(
            "RangeError", e.what(), route_pattern_, req.method(), req.path()));
#else
        nlohmann::json j{
            {"error", "Internal Server Error"},
            {"hint", "Invalid status code passed by handler. See server logs."},
            {"code", "E_INVALID_STATUS"}};
        Response::json_response(res, j, INTERNAL_ERROR);
#endif
        finalize_response(req, res);
      }
      catch (const std::exception &e)
      {
        log().log(vix::utils::Logger::Level::Error,
                  "Route '{}' threw exception: {} (method={}, path={})",
                  route_pattern_, e.what(), req.method(), req.path());

#ifndef NDEBUG
        res.set_status(INTERNAL_ERROR);
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_body(make_dev_error_html(
            "Error", e.what(), route_pattern_, req.method(), req.path()));
#else
        Response::error_response(res, INTERNAL_ERROR, "Internal Server Error");
#endif
        finalize_response(req, res);
      }

      co_return;
    }

  private:
    std::string route_pattern_;
    Handler handler_;
    vix::view::TemplateView *template_view_{nullptr};

    static void finalize_response(const Request &req, Response &res)
    {
      if (res.status() == NO_CONTENT || res.status() == NOT_MODIFIED)
      {
        res.set_body("");
      }

      if (!res.has_header("Connection"))
      {
        const std::string connection = req.header("Connection");
        if (!connection.empty())
        {
          res.set_header("Connection", connection);
          res.set_should_close(connection == "close");
        }
        else
        {
          res.set_header("Connection", "keep-alive");
          res.set_should_close(false);
        }
      }

      if (!res.has_header("Content-Length"))
      {
        res.set_header("Content-Length", std::to_string(res.body().size()));
      }

      if (!res.has_header("Server"))
      {
        res.set_header("Server", "Vix.cpp");
      }

      if (!res.has_header("Date"))
      {
        res.set_header("Date", Response::http_date_now());
      }
    }

    template <class T>
    static void maybe_auto_send(
        ResponseWrapper &wrapped,
        Response &res,
        T &&value)
    {
      if (!res.body().empty())
        return;

      using V = std::decay_t<T>;

      if constexpr (StatusPayloadPair<V>)
      {
        auto &&v = std::forward<T>(value);
        wrapped.status(static_cast<int>(v.first));

        if (res.status() == NO_CONTENT)
        {
          wrapped.send();
          return;
        }

        wrapped.send(v.second);
      }
      else if constexpr (StatusPayloadTuple<V>)
      {
        auto &&v = std::forward<T>(value);
        wrapped.status(static_cast<int>(std::get<0>(v)));

        if (res.status() == NO_CONTENT)
        {
          wrapped.send();
          return;
        }

        wrapped.send(std::get<1>(v));
      }
      else
      {
        wrapped.send(std::forward<T>(value));
      }
    }
  };

} // namespace vix::vhttp

#endif // VIX_REQUEST_HANDLER_HPP
