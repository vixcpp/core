# ======================================================
# vix.cpp Core Module - CMake Configuration (Professional)
# ======================================================
# This CMake configuration defines the build process for the `core` module of Vix.cpp,
# a high-performance modular C++ web framework.
# The core module provides the main HTTP server, routing system, middleware pipeline,
# configuration management, and session handling.
# ======================================================

cmake_minimum_required(VERSION 3.20)
project(vix_core VERSION 0.2.0 LANGUAGES CXX)

# ======================================================
# Compiler settings
# ======================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Global warning and optimization flags
set(GLOBAL_CXX_FLAGS "-Wall -Wextra -Wshadow -Weffc++")
set(CMAKE_CXX_FLAGS_RELEASE "${GLOBAL_CXX_FLAGS} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${GLOBAL_CXX_FLAGS} -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ======================================================
# Build options
# ======================================================
option(VIX_CORE_WITH_OPENSSL "Enable HTTPS/TLS support using OpenSSL" ON)
option(VIX_CORE_WITH_MYSQL   "Enable MySQL client linkage" OFF)

# ======================================================
# External dependencies
# ======================================================
find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system)
find_package(spdlog REQUIRED)   # Provided transitively by utils, but kept for standalone builds

if (VIX_CORE_WITH_OPENSSL)
  find_package(OpenSSL REQUIRED)
endif()

if (VIX_CORE_WITH_MYSQL)
  find_package(MySQL REQUIRED COMPONENTS CppConn) 
endif()

# ======================================================
# Source files
# ======================================================
set(CORE_SOURCES
  app/App.cpp
  app/Module.cpp
  server/HTTPServer.cpp
  server/ThreadPool.cpp
  router/Router.cpp
  http/Request.cpp
  http/Response.cpp
  middleware/Middleware.cpp
  middleware/MiddlewarePipeline.cpp
  middleware/MiddlewareContext.cpp
  config/Config.cpp
  session/Session.cpp
)

# ======================================================
# Core library target
# ======================================================
add_library(core STATIC ${CORE_SOURCES})
add_library(Vix::core ALIAS core)

target_compile_features(core PUBLIC cxx_std_20)

target_include_directories(core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/router
    ${CMAKE_CURRENT_SOURCE_DIR}/http
    ${CMAKE_CURRENT_SOURCE_DIR}/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/session
)

# Resolve JSON target name (supports several variants)
set(JSON_TARGET "Vix::json")
if (NOT TARGET ${JSON_TARGET})
  if (TARGET vix_json)
    set(JSON_TARGET vix_json)
  elseif (TARGET vix::json)
    # If someone exported a lowercase alias
    set(JSON_TARGET vix::json)
  else()
    message(FATAL_ERROR
      "JSON module target not found. Expected Vix::json (or vix_json / vix::json).\n"
      "Make sure 'modules/json' is added and defines 'add_library(Vix::json ALIAS vix_json)'.")
  endif()
endif()


# ======================================================
# Linking configuration
# ======================================================
# The core module depends on:
#   - Vix::utils: logging, utilities, and helpers (includes spdlog)
#   - Vix::json: lightweight JSON abstraction layer (header-only)
#   - Boost: filesystem and system components
target_link_libraries(core
  PUBLIC
    Vix::utils
    ${JSON_TARGET}
    Boost::filesystem
    Boost::system
)

if (VIX_CORE_WITH_OPENSSL)
  target_link_libraries(core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

if (VIX_CORE_WITH_MYSQL)
  target_link_libraries(core PRIVATE mysqlcppconn)
endif()

# ======================================================
# Library metadata and export configuration
# ======================================================
set_target_properties(core PROPERTIES
  OUTPUT_NAME vix_core
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  EXPORT_NAME core
)

# ======================================================
# Status summary
# ======================================================
message(STATUS "------------------------------------------------------")
message(STATUS "Vix::core module configured successfully")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "OpenSSL enabled: ${VIX_CORE_WITH_OPENSSL}")
message(STATUS "MySQL linkage:   ${VIX_CORE_WITH_MYSQL}")
message(STATUS "------------------------------------------------------")
