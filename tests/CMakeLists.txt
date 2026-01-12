cmake_minimum_required(VERSION 3.20)

include(CTest)

if (BUILD_TESTING)
  enable_testing()
endif()

set(VIX_CORE_TEST_TARGET vix::core)
if (NOT TARGET ${VIX_CORE_TEST_TARGET} AND TARGET vix_core)
  set(VIX_CORE_TEST_TARGET vix_core)
endif()

if (NOT TARGET ${VIX_CORE_TEST_TARGET})
  message(FATAL_ERROR "[core/tests] Missing core target (expected vix::core or vix_core).")
endif()

add_executable(core_smoke_test
  core_smoke_test.cpp
)

target_link_libraries(core_smoke_test PRIVATE
  ${VIX_CORE_TEST_TARGET}
)

target_compile_features(core_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(NAME core_smoke_test COMMAND core_smoke_test)
endif()
