cmake_minimum_required(VERSION 3.20)

# Enable CTest
include(CTest)

if (BUILD_TESTING)
  enable_testing()
endif()

# ----------------------------
# Sync / Outbox smoke test
# ----------------------------
add_executable(core_sync_smoke_test
  sync/sync_engine_outbox_smoke_test.cpp
)

target_link_libraries(core_sync_smoke_test PRIVATE
  vix_core
)

target_compile_features(core_sync_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(
    NAME core_sync_smoke_test
    COMMAND core_sync_smoke_test
  )
endif()

# ----------------------------
# Sync / Outbox permanent failure test (F1)
# ----------------------------
add_executable(core_sync_permanent_fail_test
  sync/sync_engine_outbox_permanent_fail_test.cpp
)

target_link_libraries(core_sync_permanent_fail_test PRIVATE
  vix_core
)

target_compile_features(core_sync_permanent_fail_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(
    NAME core_sync_permanent_fail_test
    COMMAND core_sync_permanent_fail_test
  )
endif()

# ----------------------------
# HTTP Cache smoke test (Memory + File)
# ----------------------------
add_executable(core_http_cache_smoke_test
  http/cache_smoke_test.cpp
)

target_link_libraries(core_http_cache_smoke_test PRIVATE
  vix_core
)

target_compile_features(core_http_cache_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(
    NAME core_http_cache_smoke_test
    COMMAND core_http_cache_smoke_test
  )
endif()

# ----------------------------
# HTTP CacheContextMapper smoke test
# ----------------------------
add_executable(core_http_cache_context_mapper_smoke_test
  http/cache_context_mapper_smoke_test.cpp
)

target_link_libraries(core_http_cache_context_mapper_smoke_test PRIVATE
  vix_core
)

target_compile_features(core_http_cache_context_mapper_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(
    NAME core_http_cache_context_mapper_smoke_test
    COMMAND core_http_cache_context_mapper_smoke_test
  )
endif()
