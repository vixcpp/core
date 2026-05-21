# ====================================================================
#  @file CMakeLists.txt
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
#   - vix::async
#   - vix::env
#   - vix::io
#   - vix::json  (resolved dynamically among common aliases)
#
# Additional Dependencies in STATIC mode:
#   - OpenSSL::SSL, OpenSSL::Crypto (optional via VIX_CORE_WITH_OPENSSL)
#   - MySQL client (optional via VIX_CORE_WITH_MYSQL; detected as
#     mysqlcppconn8 or mysqlcppconn)
#
# Options:
#   - VIX_CORE_WITH_OPENSSL : Enable TLS (default ON)
#   - VIX_CORE_WITH_MYSQL   : Link MySQL client (default OFF)
#   - VIX_ENABLE_SANITIZERS : Inherit sanitizers from the parent project
#
# Installation/Export:
#   - Umbrella build exports into: VixTargets
#   - Standalone build exports into: vix_coreTargets
# ====================================================================

cmake_minimum_required(VERSION 3.20)
project(vix_core VERSION 1.6.2 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# --------------------------------------------------------------------
# Global settings
# --------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --------------------------------------------------------------------
# Export set selection
# --------------------------------------------------------------------
if (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD)
  set(VIX_CORE_EXPORT_SET VixTargets)
else()
  set(VIX_CORE_EXPORT_SET vix_coreTargets)
endif()

option(VIX_CORE_WITH_OPENSSL "Enable HTTPS/TLS support using OpenSSL" ON)
option(VIX_CORE_WITH_MYSQL   "Enable MySQL client linkage" OFF)
option(VIX_CORE_FETCH_UTILS  "Auto-resolve vix::utils if missing" ON)
option(VIX_CORE_FETCH_ASYNC  "Auto-resolve vix::async if missing" ON)
option(VIX_CORE_FETCH_ENV    "Auto-resolve vix::env if missing" ON)
option(VIX_CORE_FETCH_IO     "Auto-resolve vix::io if missing" ON)
option(VIX_BENCH_MODE        "Enable ultra-minimal /bench route for benchmarking" OFF)
option(VIX_CORE_FETCH_TEMPLATE "Auto-resolve vix::template if missing" ON)
set(VIX_CORE_WITH_TEMPLATE "AUTO" CACHE STRING "Link template backend (AUTO|ON|OFF)")
set_property(CACHE VIX_CORE_WITH_TEMPLATE PROPERTY STRINGS AUTO ON OFF)

# Sources discovery
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# --------------------------------------------------------------------
# utils dependency
# --------------------------------------------------------------------
if (NOT TARGET vix::utils AND NOT TARGET vix_utils)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../utils/CMakeLists.txt")
    message(STATUS "[core] Adding utils from sibling: ../utils")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../utils" "utils")
  elseif (VIX_CORE_FETCH_UTILS)
    find_package(vix_utils CONFIG QUIET)
    find_package(utils CONFIG QUIET)
  endif()
endif()

set(VIX_UTILS_TARGET "")
if (TARGET vix::utils)
  set(VIX_UTILS_TARGET vix::utils)
elseif (TARGET vix_utils)
  set(VIX_UTILS_TARGET vix_utils)
endif()

if (NOT VIX_UTILS_TARGET)
  message(FATAL_ERROR
    "vix::utils not found.\n"
    "Provide ../utils in the umbrella build, or install/export the utils package."
  )
endif()

# --------------------------------------------------------------------
# async dependency
# --------------------------------------------------------------------
if (NOT TARGET vix::async AND NOT TARGET vix_async)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../async/CMakeLists.txt")
    message(STATUS "[core] Adding async from sibling: ../async")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../async" "async")
  elseif (VIX_CORE_FETCH_ASYNC)
    find_package(vix_async CONFIG QUIET)
    find_package(async CONFIG QUIET)
  endif()
endif()

set(VIX_ASYNC_TARGET "")
if (TARGET vix::async)
  set(VIX_ASYNC_TARGET vix::async)
elseif (TARGET vix_async)
  set(VIX_ASYNC_TARGET vix_async)
endif()

if (NOT VIX_ASYNC_TARGET)
  message(FATAL_ERROR
    "vix::async not found.\n"
    "Provide ../async in the umbrella build, or install/export the async package."
  )
endif()

# --------------------------------------------------------------------
# env dependency
# --------------------------------------------------------------------
if (NOT TARGET vix::env AND NOT TARGET vix_env)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../env/CMakeLists.txt")
    message(STATUS "[core] Adding env from sibling: ../env")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../env" "env")
  elseif (VIX_CORE_FETCH_ENV)
    find_package(vix_env CONFIG QUIET)
    find_package(env CONFIG QUIET)
  endif()
endif()

set(VIX_ENV_TARGET "")
if (TARGET vix::env)
  set(VIX_ENV_TARGET vix::env)
elseif (TARGET vix_env)
  set(VIX_ENV_TARGET vix_env)
endif()

if (NOT VIX_ENV_TARGET)
  message(FATAL_ERROR
    "vix::env not found.\n"
    "Provide ../env in the umbrella build, or install/export the env package."
  )
endif()

# --------------------------------------------------------------------
# io dependency
# --------------------------------------------------------------------
if (NOT TARGET vix::io AND NOT TARGET vix_io)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../io/CMakeLists.txt")
    message(STATUS "[core] Adding io from sibling: ../io")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../io" "io")
  elseif (VIX_CORE_FETCH_IO)
    find_package(vix_io CONFIG QUIET)
    find_package(io CONFIG QUIET)
  endif()
endif()

set(VIX_IO_TARGET "")
if (TARGET vix::io)
  set(VIX_IO_TARGET vix::io)
elseif (TARGET vix_io)
  set(VIX_IO_TARGET vix_io)
endif()

if (NOT VIX_IO_TARGET)
  message(FATAL_ERROR
    "vix::io not found.\n"
    "Provide ../io in the umbrella build, or install/export the io package."
  )
endif()

# --------------------------------------------------------------------
# JSON linkage policy
# --------------------------------------------------------------------
set(VIX_CORE_WITH_JSON "AUTO" CACHE STRING "Link JSON backend (AUTO|ON|OFF)")
set_property(CACHE VIX_CORE_WITH_JSON PROPERTY STRINGS AUTO ON OFF)

function(vix_core_try_link_json onto link_scope)
  if (VIX_CORE_WITH_JSON STREQUAL "OFF")
    message(STATUS "[core] JSON disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_JSON=1)
    set(VIX_CORE_PACKAGE_WITH_JSON OFF PARENT_SCOPE)
    return()
  endif()

  set(_json_candidates
    vix::json
    vix_json
  )

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
    set(VIX_CORE_PACKAGE_WITH_JSON ON PARENT_SCOPE)
  else()
    if (VIX_CORE_WITH_JSON STREQUAL "ON")
      message(FATAL_ERROR
        "JSON required but not found (expected: vix::json or vix_json)."
      )
    else()
      message(STATUS "[core] No JSON backend detected; building without JSON.")
      target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_JSON=1)
      set(VIX_CORE_PACKAGE_WITH_JSON OFF PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(vix_core_try_link_template onto link_scope)
  if (VIX_CORE_WITH_TEMPLATE STREQUAL "OFF")
    message(STATUS "[core] Template disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_TEMPLATE=1)
    set(VIX_CORE_PACKAGE_WITH_TEMPLATE OFF PARENT_SCOPE)
    return()
  endif()

  set(_template_candidates
    vix::template
    template
  )

  set(_template_found "")
  foreach(t IN LISTS _template_candidates)
    if (TARGET ${t})
      set(_template_found ${t})
      break()
    endif()
  endforeach()

  if (_template_found)
    message(STATUS "[core] Template backend: ${_template_found}")
    target_link_libraries(${onto} ${link_scope} ${_template_found})
    target_compile_definitions(${onto} ${link_scope} VIX_CORE_HAS_TEMPLATE=1)
    set(VIX_CORE_PACKAGE_WITH_TEMPLATE ON PARENT_SCOPE)
  else()
    if (VIX_CORE_WITH_TEMPLATE STREQUAL "ON")
      message(FATAL_ERROR
        "Template required but not found (expected: vix::template or template)."
      )
    else()
      message(STATUS "[core] No template backend detected; building without template.")
      target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_TEMPLATE=1)
      set(VIX_CORE_PACKAGE_WITH_TEMPLATE OFF PARENT_SCOPE)
    endif()
  endif()
endfunction()

# --------------------------------------------------------------------
# template dependency
# --------------------------------------------------------------------
if (NOT TARGET vix::template AND NOT TARGET template)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../template/CMakeLists.txt")
    message(STATUS "[core] Adding template from sibling: ../template")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../template" "template")
  elseif (VIX_CORE_FETCH_TEMPLATE)
    find_package(template CONFIG QUIET)
    find_package(vix_template CONFIG QUIET)
  endif()
endif()

set(VIX_TEMPLATE_TARGET "")
if (TARGET vix::template)
  set(VIX_TEMPLATE_TARGET vix::template)
elseif (TARGET template)
  set(VIX_TEMPLATE_TARGET template)
endif()

# Defaults for standalone package config generation
set(VIX_CORE_PACKAGE_WITH_OPENSSL OFF)
set(VIX_CORE_PACKAGE_WITH_JSON OFF)
set(VIX_CORE_PACKAGE_WITH_TEMPLATE OFF)

# ============================== STATIC ===============================
if (CORE_SOURCES)
  message(STATUS "[core] Building STATIC library with detected sources.")

  add_library(vix_core STATIC ${CORE_SOURCES})
  add_library(vix::core ALIAS vix_core)

  target_compile_features(vix_core PUBLIC cxx_std_20)

  target_include_directories(vix_core
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_core
    PUBLIC
      ${VIX_UTILS_TARGET}
      ${VIX_ASYNC_TARGET}
      ${VIX_ENV_TARGET}
      ${VIX_IO_TARGET}
  )

  # OpenSSL (optional)
  if (VIX_CORE_WITH_OPENSSL)
    find_package(OpenSSL QUIET)

    if (OpenSSL_FOUND)
      target_compile_definitions(vix_core PUBLIC VIX_CORE_HAS_OPENSSL=1)
      target_link_libraries(vix_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
      set(VIX_CORE_PACKAGE_WITH_OPENSSL ON)
    else()
      message(WARNING "OpenSSL requested but not found, building core without TLS")
      target_compile_definitions(vix_core PUBLIC VIX_CORE_HAS_OPENSSL=0)
      set(VIX_CORE_PACKAGE_WITH_OPENSSL OFF)
    endif()
  else()
    target_compile_definitions(vix_core PUBLIC VIX_CORE_HAS_OPENSSL=0)
    set(VIX_CORE_PACKAGE_WITH_OPENSSL OFF)
  endif()

  # Build-only helpers must not leak into exported interfaces.
  if (TARGET vix_warnings)
    message(STATUS "[core] vix_warnings detected")
    target_link_libraries(vix_core PRIVATE
      $<BUILD_INTERFACE:vix_warnings>
    )
  else()
    message(STATUS "[core] vix_warnings not found (building core standalone)")
  endif()

  if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
    target_link_libraries(vix_core PRIVATE
      $<BUILD_INTERFACE:vix_sanitizers>
    )
  endif()

  vix_core_try_link_json(vix_core PUBLIC)
  vix_core_try_link_template(vix_core PUBLIC)

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
        message(FATAL_ERROR
          "MySQL requested but neither mysqlcppconn8 nor mysqlcppconn found."
        )
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
    EXPORT ${VIX_CORE_EXPORT_SET}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

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
    ${VIX_ASYNC_TARGET}
    ${VIX_ENV_TARGET}
    ${VIX_IO_TARGET}
  )

  vix_core_try_link_json(vix_core INTERFACE)
  vix_core_try_link_template(vix_core INTERFACE)

  target_compile_definitions(vix_core INTERFACE
    VIX_CORE_WITH_MYSQL=0
    VIX_CORE_HAS_OPENSSL=0
  )

  install(TARGETS vix_core
    EXPORT ${VIX_CORE_EXPORT_SET}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

# --------------------------------------------------------------------
# Install headers
# --------------------------------------------------------------------
if (NOT (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD))
  install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.h"
  )
endif()

# --------------------------------------------------------------------
# Standalone package config/export
# Only for non-umbrella builds
# --------------------------------------------------------------------
if (NOT (DEFINED VIX_UMBRELLA_BUILD AND VIX_UMBRELLA_BUILD))

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vix_coreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vix_coreConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vix_core"
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/vix_coreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/vix_coreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/vix_coreConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vix_core"
  )

  install(EXPORT vix_coreTargets
    FILE vix_coreTargets.cmake
    NAMESPACE vix::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vix_core"
  )

endif()

# --------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------
option(VIX_CORE_BUILD_TESTS "Build core module tests" OFF)

if (VIX_CORE_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

# --------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------
message(STATUS "------------------------------------------------------")
message(STATUS "vix::core configured (${PROJECT_VERSION})")
if (CORE_SOURCES)
  message(STATUS "Mode: STATIC / sources found")
else()
  message(STATUS "Mode: HEADER-ONLY / no sources")
endif()
message(STATUS "Export set:       ${VIX_CORE_EXPORT_SET}")
message(STATUS "OpenSSL enabled:  ${VIX_CORE_WITH_OPENSSL}")
message(STATUS "MySQL linkage:    ${VIX_CORE_WITH_MYSQL}")
message(STATUS "Env target:       ${VIX_ENV_TARGET}")
message(STATUS "IO target:        ${VIX_IO_TARGET}")
message(STATUS "JSON policy:      ${VIX_CORE_WITH_JSON}")
message(STATUS "Utils target:     ${VIX_UTILS_TARGET}")
message(STATUS "Async target:     ${VIX_ASYNC_TARGET}")
message(STATUS "Include dir:      ${CMAKE_CURRENT_SOURCE_DIR}/include (for <vix/...>)")
message(STATUS "Sanitizers:       ${VIX_ENABLE_SANITIZERS}")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
