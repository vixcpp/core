/**
 * @file Version.hpp
 *
 * Central version metadata for the Vix core runtime.
 */
#ifndef VIX_CORE_VERSION_HPP
#define VIX_CORE_VERSION_HPP

#include <string_view>

#ifndef VIX_CORE_RUNTIME_VERSION
#define VIX_CORE_RUNTIME_VERSION "v2.9.0"
#endif

namespace vix
{
  inline constexpr std::string_view VERSION = VIX_CORE_RUNTIME_VERSION;
  inline constexpr std::string_view CORE_VERSION = VERSION;
}

#endif // VIX_CORE_VERSION_HPP
