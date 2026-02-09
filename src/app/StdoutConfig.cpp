/**
 *
 *  @file StdoutConfig.cpp
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
#include <iostream>
#include <cstdlib>
#include <string_view>
#include <vix/utils/Env.hpp>

namespace
{
  struct VixStdoutConfigurator
  {
    VixStdoutConfigurator()
    {
      const char *env = vix::utils::vix_getenv("VIX_STDOUT_MODE");
      if (!env)
        return;

      std::string_view mode{env};

      if (mode == "line")
      {
        std::cout << std::unitbuf;
      }
    }
  };

  VixStdoutConfigurator g_vixStdoutConfigurator;
} // namespace
