#  @file CMakeLists.cpp
#  @author Gaspard Kirira
#
#  Copyright 2025, Gaspard Kirira.  All rights reserved.
#  https://github.com/vixcpp/vix
#  Use of this source code is governed by a MIT license
#  that can be found in the License file.
#
#  Vix.cpp
#
# Purpose:
#   Build configuration for the Vix core module (HTTP server, routing,
#   configuration, sessions, etc.). If any sources are found under ./src,
#   we build a STATIC library; otherwise we fall back to a header-only
#   INTERFACE target (useful for embedding or quick prototyping).
#
# Public Targets:
#   - vix_core   : The actual library target (STATIC or INTERFACE)
#   - vix::core  : Namespaced alias for consumers
#
# Public Dependencies:
#   - vix::utils
#   - vix::json  (resolved dynamically among common aliases)
#
# Additional Dependencies in STATIC mode:
#   - Boost::filesystem, Boost::system
#   - OpenSSL::SSL, OpenSSL::Crypto (optional via VIX_CORE_WITH_OPENSSL)
#   - MySQL client (optional via VIX_CORE_WITH_MYSQL; detected as mysqlcppconn8 or mysqlcppconn)
#
# Options:
#   - VIX_CORE_WITH_OPENSSL : Enable TLS (default ON)
#   - VIX_CORE_WITH_MYSQL   : Link MySQL client (default OFF)
#   - VIX_ENABLE_SANITIZERS : Inherit sanitizers from the parent project
#
# Installation/Export:
#   This module installs its target and headers and contributes to the
#   top-level export-set `VixTargets`. Consumers can then use:
#
#       find_package(Vix CONFIG REQUIRED)
#       target_link_libraries(app PRIVATE vix::core)
#
# Notes:
#   - This file is designed to be included from a superproject (preferred).
#   - Avoids per-module export-sets; relies on the umbrella’s `VixTargets`.
#   - The JSON backend target is selected dynamically (vix::json / vix::json / vix_json).
# ====================================================================

cmake_minimum_required(VERSION 3.20)
project(vix_core VERSION 1.6.2 LANGUAGES CXX)

include(GNUInstallDirs)

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(VIX_CORE_WITH_OPENSSL "Enable HTTPS/TLS support using OpenSSL" ON)
option(VIX_CORE_WITH_MYSQL   "Enable MySQL client linkage" OFF)
option(VIX_BENCH_MODE "Enable ultra-minimal /bench route for benchmarking" OFF)

# Sources discovery
# If any .cpp exists under src/, we switch to STATIC build mode.
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Utils dependency
option(VIX_CORE_FETCH_UTILS "Auto-fetch vix::utils if missing" ON)

if (NOT TARGET vix::utils)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../utils/CMakeLists.txt")
    message(STATUS "[core] Adding utils from umbrella: ../utils")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../utils" "utils")
  elseif (VIX_CORE_FETCH_UTILS)
    include(FetchContent)
    message(STATUS "[core] Fetching vix::utils via FetchContent")
    FetchContent_Declare(vix_utils
      GIT_REPOSITORY https://github.com/vixcpp/utils.git
      GIT_TAG       dev
    )
    FetchContent_MakeAvailable(vix_utils)
  else()
    message(FATAL_ERROR "vix::utils not found. Enable VIX_CORE_FETCH_UTILS=ON or provide the target before core.")
  endif()
endif()

set(VIX_UTILS_TARGET vix::utils)
if (NOT TARGET ${VIX_UTILS_TARGET} AND TARGET vix::utils)
  set(VIX_UTILS_TARGET vix::utils)
endif()

# JSON linkage policy
set(VIX_CORE_WITH_JSON "AUTO" CACHE STRING "Link JSON backend (AUTO|ON|OFF)")
set_property(CACHE VIX_CORE_WITH_JSON PROPERTY STRINGS AUTO ON OFF)

function(vix_core_try_link_json onto link_scope)
  if (VIX_CORE_WITH_JSON STREQUAL "OFF")
    message(STATUS "[core] JSON disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_JSON=1)
    return()
  endif()

  set(_json_candidates vix::json vix_json)
  set(_json_found "")
  foreach(t IN LISTS _json_candidates)
    if (TARGET ${t})
      set(_json_found ${t})
      break()
    endif()
  endforeach()

  if (_json_found)
    message(STATUS "[core] JSON backend: ${_json_found}")
    target_link_libraries(${onto} ${link_scope} ${_json_found})
  else()
    if (VIX_CORE_WITH_JSON STREQUAL "ON")
      message(FATAL_ERROR "JSON required but not found (expected: vix::json, vix::json, or vix_json).")
    else()
      message(STATUS "[core] No JSON backend detected; building without JSON.")
      target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_JSON=1)
    endif()
  endif()
endfunction()

# Boost (required because public headers include Boost.Beast)
set(VIX_CORE_WITH_BOOST "ON" CACHE STRING "Use Boost (ON|OFF)")
set_property(CACHE VIX_CORE_WITH_BOOST PROPERTY STRINGS ON OFF)

if (VIX_CORE_WITH_BOOST STREQUAL "ON")
  # Prefer CONFIG packages when available (Homebrew), then fallback.
  set(Boost_NO_WARN_NEW_VERSIONS ON)

  find_package(Boost CONFIG QUIET)

  if (Boost_FOUND)
    message(STATUS "[core] Boost found (CONFIG)")
    # Homebrew may not provide Boost::system as a CMake package component.
    # We will link system via the library file later if needed.
  else()
    message(STATUS "[core] Boost not found via CONFIG, using FindBoost")
    find_package(Boost REQUIRED COMPONENTS system)
  endif()

  # ⚠️ macOS: detect mixed Homebrew Boost installs
  if (APPLE)
    if (EXISTS "/usr/local/include/boost" AND EXISTS "/opt/homebrew/include/boost")
      message(WARNING
        "[core] Both /usr/local and /opt/homebrew Boost detected. "
        "Prefer one to avoid mixed includes.")
    endif()
  endif()

  # Normalize headers target across toolchains
  if (TARGET Boost::headers AND NOT TARGET Boost::boost)
    add_library(Boost::boost ALIAS Boost::headers)
  endif()

  # Collect Boost include dirs (works for both CONFIG and FindBoost)
  set(_vix_boost_includes "")

  # FindBoost path
  if (Boost_INCLUDE_DIRS)
    list(APPEND _vix_boost_includes ${Boost_INCLUDE_DIRS})
  endif()

  # CONFIG target path
  if (TARGET Boost::boost)
    get_target_property(_boost_target_includes Boost::boost INTERFACE_INCLUDE_DIRECTORIES)
    if (_boost_target_includes)
      list(APPEND _vix_boost_includes ${_boost_target_includes})
    endif()
  endif()

  list(REMOVE_DUPLICATES _vix_boost_includes)

else()
  message(FATAL_ERROR "[core] Boost is required because core public headers include Boost.Beast. Set VIX_CORE_WITH_BOOST=ON.")
endif()

# ============================== STATIC ===============================
if (CORE_SOURCES)
  message(STATUS "[core] Building STATIC library with detected sources.")

  add_library(vix_core STATIC ${CORE_SOURCES})
  add_library(vix::core ALIAS vix_core)
  target_compile_features(vix_core PUBLIC cxx_std_20)

  # OpenSSL (optional)
  if (VIX_CORE_WITH_OPENSSL)
    find_package(OpenSSL QUIET)

    if (OpenSSL_FOUND)
      target_compile_definitions(vix_core PUBLIC VIX_CORE_HAS_OPENSSL=1)
      target_link_libraries(vix_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    else()
      message(WARNING "OpenSSL requested but not found, building core without TLS")
      target_compile_definitions(vix_core PUBLIC VIX_CORE_HAS_OPENSSL=0)
    endif()
  else()
    target_compile_definitions(vix_core PUBLIC VIX_CORE_HAS_OPENSSL=0)
  endif()

  if (TARGET vix_warnings)
    message(STATUS "[core] vix_warnings detected (from umbrella)")
    target_link_libraries(vix_core PUBLIC vix_warnings)
    if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
      target_link_libraries(vix_core INTERFACE vix_sanitizers)
    endif()

  else()
    message(STATUS "[core] vix_warnings not found (building core standalone)")
  endif()

  target_include_directories(vix_core
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_core
    PUBLIC
      ${VIX_UTILS_TARGET}
  )

  target_link_libraries(vix_core PUBLIC Boost::boost)

  # Treat Boost headers as SYSTEM (avoid third-party warnings)
  if (_vix_boost_includes)
    target_include_directories(vix_core SYSTEM PUBLIC ${_vix_boost_includes})
  endif()


  if (TARGET Boost::system)
    target_link_libraries(vix_core PUBLIC Boost::system)
  elseif (TARGET boost_system)
    target_link_libraries(vix_core PUBLIC boost_system)
  else()
    if (DEFINED Boost_SYSTEM_LIBRARY AND Boost_SYSTEM_LIBRARY)
      target_link_libraries(vix_core PUBLIC ${Boost_SYSTEM_LIBRARY})
    else()
      target_link_libraries(vix_core PUBLIC boost_system)
    endif()
  endif()


  vix_core_try_link_json(vix_core PUBLIC)

  if (VIX_CORE_WITH_MYSQL)
    find_library(MYSQLCPP_CONN8_LIB mysqlcppconn8)
    if (MYSQLCPP_CONN8_LIB)
      target_link_libraries(vix_core PRIVATE ${MYSQLCPP_CONN8_LIB})
      target_compile_definitions(vix_core PUBLIC VIX_CORE_WITH_MYSQL=1)
    else()
      find_library(MYSQLCPP_CONN_LIB mysqlcppconn)
      if (MYSQLCPP_CONN_LIB)
        target_link_libraries(vix_core PRIVATE ${MYSQLCPP_CONN_LIB})
        target_compile_definitions(vix_core PUBLIC VIX_CORE_WITH_MYSQL=1)
      else()
        message(FATAL_ERROR "MySQL requested but neither mysqlcppconn8 nor mysqlcppconn found.")
      endif()
    endif()
  else()
    target_compile_definitions(vix_core PUBLIC VIX_CORE_WITH_MYSQL=0)
  endif()

  if (VIX_BENCH_MODE)
    message(STATUS "[core] Bench mode enabled: /bench route compiled in")
    target_compile_definitions(vix_core PUBLIC VIX_BENCH_MODE=1)
  endif()

  set_target_properties(vix_core PROPERTIES
    OUTPUT_NAME vix_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME core
  )

  install(TARGETS vix_core
    EXPORT VixTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

# ============================ HEADER-ONLY ============================
else()
  message(STATUS "[core] Building HEADER-ONLY library (no sources).")

  add_library(vix_core INTERFACE)
  add_library(vix::core ALIAS vix_core)
  target_compile_features(vix_core INTERFACE cxx_std_20)

  target_include_directories(vix_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_core INTERFACE
    ${VIX_UTILS_TARGET}
  )

  target_link_libraries(vix_core INTERFACE Boost::boost)

  # Treat Boost headers as SYSTEM to avoid third-party warnings breaking -Werror/-Wsign-conversion.
  if (_vix_boost_includes)
    target_include_directories(vix_core SYSTEM INTERFACE ${_vix_boost_includes})
  endif()

  if (TARGET Boost::system)
    target_link_libraries(vix_core INTERFACE Boost::system)
  elseif (TARGET boost_system)
    target_link_libraries(vix_core INTERFACE boost_system)
  else()
    if (DEFINED Boost_SYSTEM_LIBRARY AND Boost_SYSTEM_LIBRARY)
      target_link_libraries(vix_core INTERFACE ${Boost_SYSTEM_LIBRARY})
    else()
      target_link_libraries(vix_core INTERFACE boost_system)
    endif()
  endif()

  if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
    target_link_libraries(vix_core INTERFACE vix_sanitizers)
  endif()

  vix_core_try_link_json(vix_core INTERFACE)

  target_compile_definitions(vix_core INTERFACE VIX_CORE_WITH_MYSQL=0)

  install(TARGETS vix_core
    EXPORT VixTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
endif()

# ----------------------------- Tests ---------------------------------
option(VIX_CORE_BUILD_TESTS "Build core module tests" OFF)

if (VIX_CORE_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

# ----------------------------- Summary -------------------------------
message(STATUS "------------------------------------------------------")
message(STATUS "vix::core configured (${PROJECT_VERSION})")
if (CORE_SOURCES)
  message(STATUS "Mode: STATIC / sources found")
else()
  message(STATUS "Mode: HEADER-ONLY / no sources")
endif()
message(STATUS "OpenSSL enabled: ${VIX_CORE_WITH_OPENSSL}")
message(STATUS "MySQL linkage:   ${VIX_CORE_WITH_MYSQL}")
message(STATUS "JSON policy:     ${VIX_CORE_WITH_JSON}")
message(STATUS "Utils target:    ${VIX_UTILS_TARGET}")
message(STATUS "Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include (for <vix/...>)")
message(STATUS "Sanitizers (inherited): ${VIX_ENABLE_SANITIZERS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
