cmake_minimum_required(VERSION 3.20)

set(VIX_CORE_BENCH_TARGET vix::core)

if (NOT TARGET ${VIX_CORE_BENCH_TARGET} AND TARGET vix_core)
  set(VIX_CORE_BENCH_TARGET vix_core)
endif()

if (NOT TARGET ${VIX_CORE_BENCH_TARGET})
  message(FATAL_ERROR "[core/benchmarks] Missing core target (expected vix::core or vix_core).")
endif()

function(vix_core_add_benchmark bench_name)
  add_executable(${bench_name}
    ${ARGN}
  )

  target_link_libraries(${bench_name} PRIVATE
    ${VIX_CORE_BENCH_TARGET}
  )

  target_compile_features(${bench_name} PRIVATE cxx_std_20)

  target_include_directories(${bench_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
  )

  if (MSVC)
    target_compile_options(${bench_name} PRIVATE /O2)
  else()
    target_compile_options(${bench_name} PRIVATE -O3)
  endif()

  set_target_properties(${bench_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks/core
  )
endfunction()

vix_core_add_benchmark(core_runtime_task_bench
  runtime/runtime_task_bench.cpp
)

vix_core_add_benchmark(core_runtime_queue_bench
  runtime/runtime_queue_bench.cpp
)

vix_core_add_benchmark(core_runtime_scheduler_bench
  runtime/runtime_scheduler_bench.cpp
)

vix_core_add_benchmark(core_runtime_worker_bench
  runtime/runtime_worker_bench.cpp
)

vix_core_add_benchmark(core_executor_submit_bench
  executor/executor_submit_bench.cpp
)

vix_core_add_benchmark(core_executor_post_bench
  executor/executor_post_bench.cpp
)

vix_core_add_benchmark(core_executor_metrics_bench
  executor/executor_metrics_bench.cpp
)

vix_core_add_benchmark(core_router_match_bench
  router/router_match_bench.cpp
)

vix_core_add_benchmark(core_router_registration_bench
  router/router_registration_bench.cpp
)

vix_core_add_benchmark(core_http_request_bench
  http/http_request_bench.cpp
)

vix_core_add_benchmark(core_http_response_bench
  http/http_response_bench.cpp
)

vix_core_add_benchmark(core_session_fake_transport_bench
  session/session_fake_transport_bench.cpp
)

vix_core_add_benchmark(core_app_route_registration_bench
  app/app_route_registration_bench.cpp
)

vix_core_add_benchmark(core_app_group_registration_bench
  app/app_group_registration_bench.cpp
)

add_custom_target(core_benchmarks
  DEPENDS
    core_runtime_task_bench
    core_runtime_queue_bench
    core_runtime_scheduler_bench
    core_runtime_worker_bench
    core_executor_submit_bench
    core_executor_post_bench
    core_executor_metrics_bench
    core_router_match_bench
    core_router_registration_bench
    core_http_request_bench
    core_http_response_bench
    core_session_fake_transport_bench
    core_app_route_registration_bench
    core_app_group_registration_bench
)
