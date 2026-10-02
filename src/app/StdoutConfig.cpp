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
#include <string>

#include <vix/env/GetOr.hpp>

namespace
{
  struct VixStdoutConfigurator
  {
    VixStdoutConfigurator()
    {
      const std::string mode = vix::env::get_or("VIX_STDOUT_MODE");

      if (mode == "line")
      {
        std::cout << std::unitbuf;
      }
    }
  };

  VixStdoutConfigurator g_vixStdoutConfigurator;
} // namespace
