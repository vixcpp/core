# ======================================================

# CMake configuration for vix.cpp core module
# ======================================================

cmake_minimum_required(VERSION 3.20)

# Project definition
project(vix_core VERSION 0.1 LANGUAGES CXX)

# ======================================================
# C++ standard and general options
# ======================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ======================================================
# Build types and optimized compilation flags
# ======================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)  # Default to Release
endif()

# Common flags applied to all builds
set(GLOBAL_CXX_FLAGS "-Wall -Wextra -Weffc++ -Wshadow")

# Release-specific flags (faster compilation and optimized)
set(CMAKE_CXX_FLAGS_RELEASE "${GLOBAL_CXX_FLAGS} -O2 -Wno-unused-result")

# Debug-specific flags (sanitizers and debug info)
set(CMAKE_CXX_FLAGS_DEBUG "${GLOBAL_CXX_FLAGS} -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")

# ======================================================
# External dependencies
# ======================================================

# Boost (required for Asio/Beast)
find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Boost not found!")
endif()

# OpenSSL for HTTPS support
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# spdlog for logging
include_directories(/usr/local/include/spdlog)
link_directories(/usr/local/lib)

# MySQL Connector/C++ for ORM
include_directories(/usr/include/mysql)
link_directories(/usr/lib)

# ======================================================
# Core sources
# ======================================================

file(GLOB_RECURSE CORE_SOURCES
    "app/*.cpp"
    "server/*.cpp"
    "router/*.cpp"
    "http/*.cpp"
    "middleware/*.cpp"
    "utils/*.cpp"
    "config/*.cpp"
    "session/*.cpp"
)

# Create a static library for core
add_library(core STATIC ${CORE_SOURCES})

# Include directories for users
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}         
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/router
    ${CMAKE_CURRENT_SOURCE_DIR}/http
    ${CMAKE_CURRENT_SOURCE_DIR}/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/session
)

# Link external libraries
target_link_libraries(core PRIVATE
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog
    mysqlcppconn
)