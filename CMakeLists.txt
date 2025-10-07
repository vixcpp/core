# ====================================================================
# Vix.cpp — Core Module
# ====================================================================
# Purpose:
#   - HTTP server, routing, middleware pipeline, config, session, etc.
#   - If sources exist in ./src, build a static library.
#     If not yet implemented, fall back to header-only so the tree builds.
#
# Targets:
#   - vix_core   (real target)
#   - Vix::core  (alias, preferred by consumers)
#
# Dependencies:
#   - PUBLIC: Vix::utils, Vix::json
#   - STATIC mode adds Boost (filesystem, system), optionally OpenSSL/MySQL.
# ====================================================================

cmake_minimum_required(VERSION 3.20)
project(vix_core VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(GLOBAL_CXX_FLAGS "-Wall -Wextra -Wshadow -Weffc++")
set(CMAKE_CXX_FLAGS_RELEASE "${GLOBAL_CXX_FLAGS} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${GLOBAL_CXX_FLAGS} -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG")

option(VIX_CORE_WITH_OPENSSL "Enable HTTPS/TLS support using OpenSSL" ON)
option(VIX_CORE_WITH_MYSQL   "Enable MySQL client linkage" OFF)

# Source auto-discovery: if empty, we switch to header-only mode.
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Resolve JSON target (robust to different export names)
set(JSON_TARGET "Vix::json")
if (NOT TARGET ${JSON_TARGET})
  if (TARGET vix_json)        # fallback if alias not declared
    set(JSON_TARGET vix_json)
  else()
    message(FATAL_ERROR
      "JSON module target not found. Expected Vix::json (or vix_json). "
      "Ensure 'modules/json' defines 'Vix::json'.")
  endif()
endif()

if (CORE_SOURCES)
  # ------------------------------ STATIC mode ------------------------
  message(STATUS "[core] Building STATIC library with detected sources.")

  find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system)
  find_package(spdlog REQUIRED)  # useful for standalone builds

  if (VIX_CORE_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
  endif()

  if (VIX_CORE_WITH_MYSQL)
    # Adjust to your environment (mysqlclient/mysqlcppconn, etc.)
    find_package(MySQL REQUIRED COMPONENTS CppConn)
  endif()

  add_library(vix_core STATIC ${CORE_SOURCES})
  add_library(Vix::core ALIAS vix_core)

  target_compile_features(vix_core PUBLIC cxx_std_20)

  # Public headers (so users can #include <vix/...>)
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

  if (VIX_CORE_WITH_MYSQL)
    target_link_libraries(vix_core PRIVATE mysqlcppconn)
  endif()

  set_target_properties(vix_core PROPERTIES
    OUTPUT_NAME vix_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME core
  )

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

else()
  # --------------------------- HEADER-ONLY mode ----------------------
  message(WARNING "[core] No sources found under modules/core/src/*.cpp — building header-only INTERFACE library.")

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
endif()

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
message(STATUS "------------------------------------------------------")
