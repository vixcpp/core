/**
 *
 *  @file TemplateView.hpp
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

#ifndef VIX_VIEW_TEMPLATE_VIEW_HPP
#define VIX_VIEW_TEMPLATE_VIEW_HPP

#include <memory>
#include <string>

#include <vix/http/Response.hpp>
#include <vix/template/Context.hpp>
#include <vix/template/Engine.hpp>

namespace vix::view
{
  /**
   * @brief HTTP-integrated template rendering facade.
   *
   * TemplateView is the bridge between:
   * - the Vix runtime HTTP layer
   * - the template engine
   *
   * Responsibilities:
   * - render templates by name
   * - convert render results into native HTTP responses
   * - expose streaming rendering for large outputs
   */
  class TemplateView
  {
  public:
    /**
     * @brief Construct a TemplateView from a template engine.
     *
     * @param engine Shared template engine instance.
     */
    explicit TemplateView(
        std::shared_ptr<vix::template_::Engine> engine);

    /**
     * @brief Render a template to a string.
     *
     * @param name Template name.
     * @param context Runtime rendering context.
     * @return Rendered HTML string.
     */
    [[nodiscard]] std::string render(
        const std::string &name,
        const vix::template_::Context &context) const;

    /**
     * @brief Render a template and produce a native HTTP response.
     *
     * @param name Template name.
     * @param context Runtime rendering context.
     * @return HTTP response containing rendered HTML.
     */
    [[nodiscard]] vix::vhttp::Response render_response(
        const std::string &name,
        const vix::template_::Context &context) const;

    /**
     * @brief Render a template in streaming mode.
     *
     * @tparam Output Output sink type.
     * @param name Template name.
     * @param context Runtime rendering context.
     * @param out Output sink receiving rendered chunks.
     */
    template <typename Output>
    void render_stream(
        const std::string &name,
        const vix::template_::Context &context,
        Output &out) const
    {
      engine_->render_stream(name, context, out);
    }

    /**
     * @brief Access the underlying template engine.
     *
     * @return Shared engine instance.
     */
    [[nodiscard]] const std::shared_ptr<vix::template_::Engine> &
    engine() const noexcept;

  private:
    /**
     * @brief Shared template engine.
     */
    std::shared_ptr<vix::template_::Engine> engine_;
  };

} // namespace vix::view

#endif // VIX_VIEW_TEMPLATE_VIEW_HPP
