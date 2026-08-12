/**
 *
 *  @file ResponseWrapper.cpp
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
#include <vix/http/ResponseWrapper.hpp>

#include <vix/json/json.hpp>

#include <vix/template/Context.hpp>
#include <vix/view/TemplateView.hpp>

#ifndef VIX_CORE_NO_UI
#include <vix/ui/core/View.hpp>
#include <vix/ui/html/HtmlResponse.hpp>
#endif

namespace vix::http
{
  void ordered_json_response(Response &res, const OrderedJson &j, int status_code)
  {
    res.set_status(normalize_status(status_code));
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.set_body(vix::json::dumps_compact(j));
  }

  vix::json::Json token_to_json(const vix::json::token &t)
  {
    vix::json::Json result = nullptr;
    std::visit([&](auto &&value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, std::monostate>) result = nullptr;
      else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::int64_t> || std::is_same_v<T, double> || std::is_same_v<T, std::string>) result = value;
      else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::array_t>>) result = value ? array_to_json(*value) : vix::json::Json(nullptr);
      else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::kvs>>) result = value ? kvs_to_json(*value) : vix::json::Json(nullptr);
    }, t.v);
    return result;
  }

  vix::json::Json array_to_json(const vix::json::array_t &arr)
  {
    vix::json::Json result = vix::json::Json::array();
    for (const auto &item : arr.elems) result.push_back(token_to_json(item));
    return result;
  }

  vix::json::Json kvs_to_json(const vix::json::kvs &list)
  {
    vix::json::Json result = vix::json::Json::object();
    const std::size_t count = list.flat.size() - (list.flat.size() % 2);
    for (std::size_t i = 0; i < count; i += 2)
      if (const auto *key = list.flat[i].as_string()) result[*key] = token_to_json(list.flat[i + 1]);
    return result;
  }

  nlohmann::json token_to_nlohmann(const vix::json::token &t) { return token_to_json(t); }
  nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list) { return kvs_to_json(list); }

  ResponseWrapper &ResponseWrapper::json(const vix::json::Json &j)
  {
    ensure_status();
    const int status_code = res.status();
    if (status_code == NO_CONTENT || status_code == NOT_MODIFIED) return send();
    if (!has_header("Content-Type")) { type("application/json; charset=utf-8"); header("X-Content-Type-Options", "nosniff"); }
    Response::json_response(res, j, status_code);
    return *this;
  }
  ResponseWrapper &ResponseWrapper::json(const vix::json::kvs &value) { return json(kvs_to_json(value)); }
  ResponseWrapper &ResponseWrapper::json(const vix::json::token &value) { return json(token_to_json(value)); }
  ResponseWrapper &ResponseWrapper::json(const vix::json::array_t &value) { return json(array_to_json(value)); }
  ResponseWrapper &ResponseWrapper::json(std::initializer_list<vix::json::token> value) { return json(vix::json::kvs{value}); }
  ResponseWrapper &ResponseWrapper::json_ordered(const OrderedJson &j)
  {
    ensure_status();
    const int status_code = res.status();
    if (status_code == NO_CONTENT || status_code == NOT_MODIFIED) return send();
    if (!has_header("Content-Type")) { type("application/json; charset=utf-8"); header("X-Content-Type-Options", "nosniff"); }
    ordered_json_response(res, j, status_code);
    return *this;
  }
  ResponseWrapper &ResponseWrapper::send(const vix::json::Json &value) { return json(value); }
  ResponseWrapper &ResponseWrapper::send(const vix::json::token &value) { return json(value); }
  ResponseWrapper &ResponseWrapper::send(const vix::json::array_t &value) { return json(value); }
  ResponseWrapper &ResponseWrapper::send(const vix::json::kvs &value) { return json(value); }
  ResponseWrapper &ResponseWrapper::send(std::initializer_list<vix::json::token> value) { return json(value); }
  ResponseWrapper &ResponseWrapper::send(const OrderedJson &value) { return json_ordered(value); }

  ResponseWrapper &ResponseWrapper::render(
      const std::string &name,
      const vix::template_::Context &context)
  {
    ensure_status();
    const int status_code = res.status();
    if (status_code == NO_CONTENT || status_code == NOT_MODIFIED)
      return send();
    if (!template_view_)
      throw std::runtime_error("ResponseWrapper::render() called but templates are not configured");

    auto rendered = template_view_->render_response(name, context);
    rendered.set_status(status_code);
    res = std::move(rendered);
    return *this;
  }

#ifndef VIX_CORE_NO_UI
  ResponseWrapper &ResponseWrapper::ui(const vix::ui::HtmlResponse &response)
  {
    status(response.status_code());
    const int status_code = res.status();
    if (status_code == NO_CONTENT || status_code == NOT_MODIFIED)
    {
      res.set_body("");
      return *this;
    }
    res.set_header("Content-Type", response.header_content_type());
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_body(response.body());
    return *this;
  }

  ResponseWrapper &ResponseWrapper::ui(const vix::ui::View &view)
  {
    ensure_status();
    const int status_code = res.status();
    if (status_code == NO_CONTENT || status_code == NOT_MODIFIED)
    {
      res.set_body("");
      return *this;
    }
    if (!template_view_ || !template_view_->engine())
      throw std::runtime_error("ResponseWrapper::ui() called but templates are not configured");

    return ui(vix::ui::HtmlResponse::from_view_result(
        view.render(*template_view_->engine()), status_code));
  }
#endif
} // namespace vix::http
