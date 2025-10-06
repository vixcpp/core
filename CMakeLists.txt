# ======================================================
# vix.cpp core module CMake configuration (optimized)
# ======================================================

cmake_minimum_required(VERSION 3.20)
project(vix_core VERSION 0.1 LANGUAGES CXX)

# ======================================================
# C++ standard and options
# ======================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ======================================================
# Build types
# ======================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(GLOBAL_CXX_FLAGS "-Wall -Wextra -Weffc++ -Wshadow")
set(CMAKE_CXX_FLAGS_RELEASE "${GLOBAL_CXX_FLAGS} -O2 -Wno-unused-result")
set(CMAKE_CXX_FLAGS_DEBUG "${GLOBAL_CXX_FLAGS} -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")

# ======================================================
# External dependencies
# ======================================================
find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(/usr/local/include/spdlog)
link_directories(/usr/local/lib)

include_directories(/usr/include/mysql)
link_directories(/usr/lib)

# ======================================================
# Utils library (independent, reusable)
# ======================================================
file(GLOB UTILS_SOURCES
    utils/*.cpp
)
add_library(utils STATIC ${UTILS_SOURCES})
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/utils)

# ======================================================
# Core sources
# ======================================================
set(CORE_SOURCES
    app/App.cpp
    app/Module.cpp
    server/HTTPServer.cpp
    server/ThreadPool.cpp
    router/Router.cpp
    router/IRequestHandler.hpp
    http/Request.cpp
    http/Response.cpp
    middleware/Middleware.cpp
    middleware/MiddlewarePipeline.cpp
    middleware/MiddlewareContext.cpp
    config/Config.cpp
    session/Session.cpp
)

# ======================================================
# Core library
# ======================================================
add_library(core STATIC ${CORE_SOURCES})

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/router
    ${CMAKE_CURRENT_SOURCE_DIR}/http
    ${CMAKE_CURRENT_SOURCE_DIR}/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/session
)

# ======================================================
# Link core with utils + external libraries
# ======================================================
target_link_libraries(core
    PRIVATE utils
    PRIVATE ${Boost_FILESYSTEM_LIBRARY}
    PRIVATE ${Boost_SYSTEM_LIBRARY}
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
    PRIVATE spdlog
    PRIVATE mysqlcppconn
)

# ======================================================
# Optional: Git hash tracking to skip rebuild
# ======================================================
find_package(Git QUIET)
if(GIT_FOUND)
    set(CORE_HASH_FILE "${CMAKE_CURRENT_BINARY_DIR}/core_commit_hash.txt")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE CORE_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(EXISTS ${CORE_HASH_FILE})
        file(READ ${CORE_HASH_FILE} OLD_CORE_HASH)
    else()
        set(OLD_CORE_HASH "")
    endif()

    if(NOT CORE_GIT_HASH STREQUAL OLD_CORE_HASH)
        message(STATUS "Core commit changed (${CORE_GIT_HASH}), rebuild triggered")
        file(WRITE ${CORE_HASH_FILE} "${CORE_GIT_HASH}")
    else()
        message(STATUS "Core commit unchanged (${CORE_GIT_HASH}), skipping rebuild if no source changed")
    endif()
endif()

message(STATUS "vix_core static library configured with ${CORE_SOURCES}")
message(STATUS "Utils static library configured with ${UTILS_SOURCES}")
