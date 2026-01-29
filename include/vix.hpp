/**
 *
 *  @file vix.hpp
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
#ifndef VIX_VIX_HPP
#define VIX_VIX_HPP

#include <vix/core.hpp>
#include <vix/json/json.hpp>

#if defined(VIX_HAS_MIDDLEWARE)
#include <vix/app/App.hpp>
#include <vix/middleware/module_init.hpp>

namespace vix::detail
{
  inline void register_modules_for_umbrella()
  {
    vix::App::set_module_init(&vix_middleware_module_init);
  }

  struct UmbrellaAutoInit
  {
    UmbrellaAutoInit() { register_modules_for_umbrella(); }
  };

  inline UmbrellaAutoInit g_umbrella_auto_init{};
} // namespace vix::detail
#endif

#endif
