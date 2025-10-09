# ====================================================================
# Vix.cpp — Core Module (Documented)
# ====================================================================
# Purpose:
#   Build configuration for the Vix core module (HTTP server, routing,
#   config, session, etc.). When sources exist under ./src, we build a
#   STATIC library; otherwise we fall back to a header-only INTERFACE
#   target (suitable for embedding or quick prototyping).
#
# Public Targets:
#   - vix_core   : The actual library target (STATIC or INTERFACE)
#   - Vix::core  : Namespaced alias for consumers
#
# Dependencies:
#   - PUBLIC: Vix::utils, Vix::json
#   - STATIC mode: Boost::filesystem, Boost::system (and optionally OpenSSL/MySQL)
#
# Options:
#   - VIX_CORE_WITH_OPENSSL : Enable TLS (default ON)
#   - VIX_CORE_WITH_MYSQL   : Link MySQL client (default OFF)
#   - VIX_ENABLE_SANITIZERS : Inherit sanitizers from parent project
#
# Notes:
#   - This CMake file is usable standalone or from a superproject.
#   - In standalone mode, install/export rules are provided.
#   - We select the JSON backend target dynamically (Vix::json / vix::json / vix_json).
# ====================================================================

cmake_minimum_required(VERSION 3.20)
project(vix_core VERSION 0.2.0 LANGUAGES CXX)

# ------------------------ Global settings ----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GLOBAL_CXX_FLAGS "-Wall -Wextra -Wshadow -Weffc++")
set(CMAKE_CXX_FLAGS_RELEASE "${GLOBAL_CXX_FLAGS} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${GLOBAL_CXX_FLAGS} -g")

option(VIX_CORE_WITH_OPENSSL "Enable HTTPS/TLS support using OpenSSL" ON)
option(VIX_CORE_WITH_MYSQL   "Enable MySQL client linkage" OFF)

# ------------------------ Sources discovery --------------------------
# If any .cpp exists under src/, we switch to STATIC build mode.
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# ------------------------ Resolve JSON target ------------------------
# Pick the first existing target name among common aliases.
set(JSON_TARGET "Vix::json")
if (NOT TARGET ${JSON_TARGET})
  if (TARGET vix::json)
    set(JSON_TARGET vix::json)
  elseif (TARGET vix_json)
    set(JSON_TARGET vix_json)
  else()
    message(FATAL_ERROR
      "JSON module target not found. Expected one of: Vix::json, vix::json, vix_json. "
      "Ensure modules/json defines an exported target and alias.")
  endif()
endif()

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
  # spdlog may already be brought by Vix::utils; keep for standalone builds
  find_package(spdlog REQUIRED)

  if (VIX_CORE_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
  endif()

  add_library(vix_core STATIC ${CORE_SOURCES})
  add_library(Vix::core ALIAS vix_core)
  target_compile_features(vix_core PUBLIC cxx_std_20)

  # Debug-mode safety: enable libstdc++ debug iterators if desired
  if (CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(vix_core PRIVATE -D_GLIBCXX_DEBUG)
  endif()

  # Public headers for #include <vix/...>
  target_include_directories(vix_core
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )

  target_link_libraries(vix_core
    PUBLIC
      Vix::utils
      ${JSON_TARGET}
      Boost::filesystem
      Boost::system
  )

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

  # Install/export only in standalone mode
  if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    install(TARGETS vix_core
      EXPORT vix_coreTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin
      INCLUDES DESTINATION include
    )
    install(DIRECTORY include/ DESTINATION include)
    export(EXPORT vix_coreTargets
      FILE "${CMAKE_CURRENT_BINARY_DIR}/vix_coreTargets.cmake"
      NAMESPACE Vix::
    )
  endif()

# ============================ HEADER-ONLY ============================
else()
  add_library(vix_core INTERFACE)
  add_library(Vix::core ALIAS vix_core)
  target_compile_features(vix_core INTERFACE cxx_std_20)

  target_include_directories(vix_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )

  target_link_libraries(vix_core INTERFACE
    Vix::utils
    ${JSON_TARGET}
  )

  # Define the feature macro for header-only consumers as well
  target_compile_definitions(vix_core INTERFACE VIX_CORE_WITH_MYSQL=0)

  # Propagate sanitizers to dependents if requested
  vix_core_apply_sanitizers(vix_core INTERFACE)
endif()

# ----------------------------- Summary -------------------------------
message(STATUS "------------------------------------------------------")
message(STATUS "Vix::core configured (${PROJECT_VERSION})")
if (CORE_SOURCES)
  message(STATUS "Mode: STATIC / sources found")
else()
  message(STATUS "Mode: HEADER-ONLY / no sources")
endif()
message(STATUS "OpenSSL enabled: ${VIX_CORE_WITH_OPENSSL}")
message(STATUS "MySQL linkage:   ${VIX_CORE_WITH_MYSQL}")
message(STATUS "Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include (for <vix/...>)")
message(STATUS "Sanitizers (inherited): ${VIX_ENABLE_SANITIZERS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
