/**
 * @file SwaggerAssets.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_OPENAPI_SWAGGER_ASSETS_HPP
#define VIX_OPENAPI_SWAGGER_ASSETS_HPP

#include <cstddef>

namespace vix::openapi::assets
{

  // Swagger UI CSS (embedded)
  inline constexpr unsigned char swagger_ui_css[] = {
#include "swagger_ui_css.inc"
  };

  inline constexpr std::size_t swagger_ui_css_len =
      sizeof(swagger_ui_css);

  // Swagger UI JS bundle (embedded)
  inline constexpr unsigned char swagger_ui_bundle_js[] = {
#include "swagger_ui_bundle_js.inc"
  };

  inline constexpr std::size_t swagger_ui_bundle_js_len =
      sizeof(swagger_ui_bundle_js);
}

#endif // VIX_OPENAPI_SWAGGER_ASSETS_HPP
