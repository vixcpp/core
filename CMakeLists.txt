# ====================================================================
# Vix.cpp — Core Module
# ====================================================================
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
project(vix_core VERSION 1.5.2 LANGUAGES CXX)

include(GNUInstallDirs)

# ------------------------ Global settings ----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Friendly warnings (tunable)
set(GLOBAL_CXX_FLAGS "-Wall -Wextra -Wshadow -Weffc++")
set(CMAKE_CXX_FLAGS_RELEASE "${GLOBAL_CXX_FLAGS} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${GLOBAL_CXX_FLAGS} -g")

option(VIX_CORE_WITH_OPENSSL "Enable HTTPS/TLS support using OpenSSL" ON)
option(VIX_CORE_WITH_MYSQL   "Enable MySQL client linkage" OFF)

# ------------------------ Sources discovery --------------------------
# If any .cpp exists under src/, we switch to STATIC build mode.
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# ------------------------ Utils dependency (robust) ------------------
# We need vix::utils. Try:
# 1) already defined, 2) umbrella ../utils, 3) FetchContent, else fail.
option(VIX_CORE_FETCH_UTILS "Auto-fetch vix::utils if missing" ON)

if (NOT TARGET vix::utils AND NOT TARGET vix::utils)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../utils/CMakeLists.txt")
    message(STATUS "[core] Adding utils from umbrella: ../utils")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../utils" "utils")
  elseif (VIX_CORE_FETCH_UTILS)
    include(FetchContent)
    message(STATUS "[core] Fetching vix::utils via FetchContent")
    # ⚠️ Aligne le tag avec ta release umbrella
    FetchContent_Declare(vix_utils
      GIT_REPOSITORY https://github.com/vixcpp/utils.git
      GIT_TAG        v1.7.1
    )
    FetchContent_MakeAvailable(vix_utils)
  else()
    message(FATAL_ERROR "vix::utils not found. Enable VIX_CORE_FETCH_UTILS=ON or provide the target before core.")
  endif()
endif()

# Normalize the utils target name into VIX_UTILS_TARGET
set(VIX_UTILS_TARGET vix::utils)
if (NOT TARGET ${VIX_UTILS_TARGET} AND TARGET vix::utils)
  set(VIX_UTILS_TARGET vix::utils)
endif()

# ------------------------ JSON linkage policy ------------------------
# AUTO: link if a known JSON target exists, else compile with VIX_CORE_NO_JSON
# ON  : require JSON target (error if absent)
# OFF : never link JSON, define VIX_CORE_NO_JSON
set(VIX_CORE_WITH_JSON "AUTO" CACHE STRING "Link JSON backend (AUTO|ON|OFF)")
set_property(CACHE VIX_CORE_WITH_JSON PROPERTY STRINGS AUTO ON OFF)

# Helper to optionally link JSON or define VIX_CORE_NO_JSON
function(vix_core_try_link_json onto link_scope)
  if (VIX_CORE_WITH_JSON STREQUAL "OFF")
    message(STATUS "[core] JSON disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_CORE_NO_JSON=1)
    return()
  endif()

  set(_json_candidates vix::json vix::json vix_json)
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

# ------------------------ Helper: Sanitizers -------------------------
# Apply -fsanitize flags either PRIVATELY (for STATIC) or as INTERFACE (header-only).
function(vix_core_apply_sanitizers tgt scope)
  if (VIX_ENABLE_SANITIZERS AND TARGET ${tgt})
    message(STATUS "[core] enabling sanitizers on ${tgt} (${scope})")
    if (${scope} STREQUAL "PRIVATE")
      target_compile_options(${tgt} PRIVATE -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
      target_link_options(   ${tgt} PRIVATE                       -fsanitize=address,undefined)
    else() # INTERFACE
      target_compile_options(${tgt} INTERFACE -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
      target_link_options(   ${tgt} INTERFACE                       -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

# ============================== STATIC ===============================
if (CORE_SOURCES)
  message(STATUS "[core] Building STATIC library with detected sources.")

  # Required runtime deps when compiling sources
  find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system)
  # spdlog may already be brought by vix::utils; remains harmless here
  find_package(spdlog REQUIRED)

  if (VIX_CORE_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
  endif()

  add_library(vix_core STATIC ${CORE_SOURCES})
  add_library(vix::core ALIAS vix_core)
  target_compile_features(vix_core PUBLIC cxx_std_20)

  # Optional debug-iterator safety in Debug builds (libstdc++)
  if (CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(vix_core PRIVATE -D_GLIBCXX_DEBUG)
  endif()

  # Public headers for #include <vix/...>
  target_include_directories(vix_core
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_core
    PUBLIC
      ${VIX_UTILS_TARGET}
      Boost::filesystem
      Boost::system
  )

  # JSON (optional: AUTO/ON/OFF)
  vix_core_try_link_json(vix_core PUBLIC)

  if (VIX_CORE_WITH_OPENSSL)
    target_link_libraries(vix_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  endif()

  # ----- Optional: MySQL linkage -----
  if (VIX_CORE_WITH_MYSQL)
    # Prefer Connector/C++ 8 (X DevAPI); fallback to legacy if needed
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

  # Sanitizers (only if enabled at top-level)
  vix_core_apply_sanitizers(vix_core PRIVATE)

  set_target_properties(vix_core PROPERTIES
    OUTPUT_NAME vix_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME core
  )

  # ---- INSTALL: join the umbrella export-set (VixTargets) ----
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

  # JSON (optional: AUTO/ON/OFF)
  vix_core_try_link_json(vix_core INTERFACE)

  # Feature macro parity
  target_compile_definitions(vix_core INTERFACE VIX_CORE_WITH_MYSQL=0)

  # Propagate sanitizers to dependents if requested
  vix_core_apply_sanitizers(vix_core INTERFACE)

  # ---- INSTALL: join the umbrella export-set (VixTargets) ----
  install(TARGETS vix_core
    EXPORT VixTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
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
