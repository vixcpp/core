/**
 * @file Version.hpp
 *
 * Central version metadata for the Vix core runtime.
 */
#ifndef VIX_VERSION_HPP
#define VIX_VERSION_HPP

#include <string_view>

namespace vix
{
  inline constexpr std::string_view VERSION = "v2.7.0";
  inline constexpr std::string_view CORE_VERSION = VERSION;
}

#endif // VIX_VERSION_HPP
