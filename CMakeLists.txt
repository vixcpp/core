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

find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system)
find_package(spdlog REQUIRED)

if (VIX_CORE_WITH_OPENSSL)
  find_package(OpenSSL REQUIRED)
endif()

if (VIX_CORE_WITH_MYSQL)
  find_package(MySQL REQUIRED COMPONENTS CppConn)
endif()

# ---- Sources ----
set(CORE_SOURCES
  src/app/App.cpp
  src/app/Module.cpp
  src/server/HTTPServer.cpp
  src/server/ThreadPool.cpp
  src/router/Router.cpp
  src/http/Request.cpp
  src/http/Response.cpp
  src/middleware/Middleware.cpp
  src/middleware/MiddlewarePipeline.cpp
  src/middleware/MiddlewareContext.cpp
  src/config/Config.cpp
  src/session/Session.cpp
)

# ---- Library ----
add_library(vix_core STATIC ${CORE_SOURCES})
add_library(Vix::core ALIAS vix_core)

target_compile_features(vix_core PUBLIC cxx_std_20)

target_include_directories(vix_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set(JSON_TARGET "Vix::json")
if (NOT TARGET ${JSON_TARGET})
  if (TARGET vix_json)
    set(JSON_TARGET vix_json)
  else()
    message(FATAL_ERROR
      "JSON module target not found. Expected Vix::json (or vix_json). "
      "Ensure 'modules/json' is added and defines 'Vix::json'.")
  endif()
endif()

# ---- Linking ----
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

# ---- Metadata / Install (optional) ----
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

message(STATUS "------------------------------------------------------")
message(STATUS "Vix::core module configured successfully")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "OpenSSL enabled: ${VIX_CORE_WITH_OPENSSL}")
message(STATUS "MySQL linkage:   ${VIX_CORE_WITH_MYSQL}")
message(STATUS "------------------------------------------------------")
