/**
 *
 *  @file TemplateView.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.
 *  All rights reserved.
 *  https://github.com/vixcpp/vix
 *
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */

#include <stdexcept>
#include <utility>

#include <vix/view/TemplateView.hpp>
#include <vix/http/Status.hpp>

namespace vix::view
{
  TemplateView::TemplateView(
      std::shared_ptr<vix::template_::Engine> engine)
      : engine_(std::move(engine))
  {
    if (!engine_)
    {
      throw std::invalid_argument(
          "TemplateView requires a valid template engine");
    }
  }

  std::string TemplateView::render(
      const std::string &name,
      const vix::template_::Context &context) const
  {
    return engine_->render(name, context).output;
  }

  vix::http::Response TemplateView::render_response(
      const std::string &name,
      const vix::template_::Context &context) const
  {
    auto result = engine_->render(name, context);

    vix::http::Response res;
    res.set_status(vix::http::OK);
    res.set_header("Content-Type", "text/html; charset=utf-8");
    res.set_body(std::move(result.output));

    return res;
  }

  const std::shared_ptr<vix::template_::Engine> &
  TemplateView::engine() const noexcept
  {
    return engine_;
  }

} // namespace vix::view
