cmake_minimum_required(VERSION 3.20)

# Optional but recommended for CTest
include(CTest)

# Sync / Outbox smoke test
add_executable(core_sync_smoke_test
  sync/sync_engine_outbox_smoke_test.cpp
)

target_include_directories(core_sync_smoke_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sync
)

target_link_libraries(core_sync_smoke_test PRIVATE
  vix_core
)

target_compile_features(core_sync_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(NAME core_sync_smoke_test COMMAND core_sync_smoke_test)
endif()

# HTTP Cache smoke test (Memory + File)
add_executable(core_http_cache_smoke_test
  http/cache_smoke_test.cpp
)

target_link_libraries(core_http_cache_smoke_test PRIVATE
  vix_core
)

target_compile_features(core_http_cache_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(NAME core_http_cache_smoke_test COMMAND core_http_cache_smoke_test)
endif()

add_executable(core_http_cache_context_mapper_smoke_test
  http/cache_context_mapper_smoke_test.cpp
)

target_link_libraries(core_http_cache_context_mapper_smoke_test PRIVATE
  vix_core
)

target_compile_features(core_http_cache_context_mapper_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(NAME core_http_cache_context_mapper_smoke_test
           COMMAND core_http_cache_context_mapper_smoke_test)
endif()
