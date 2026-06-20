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

set(VIX_CORE_TEST_UI_TARGET "")

if (TARGET vix::ui)
  set(VIX_CORE_TEST_UI_TARGET vix::ui)
elseif (TARGET vix_ui)
  set(VIX_CORE_TEST_UI_TARGET vix_ui)
elseif (TARGET ui)
  set(VIX_CORE_TEST_UI_TARGET ui)
endif()

function(vix_core_add_test test_name)
  add_executable(${test_name}
    ${ARGN}
  )

  target_link_libraries(${test_name} PRIVATE
    ${VIX_CORE_TEST_TARGET}
    ${VIX_CORE_TEST_UI_TARGET}
  )

  if (TARGET vix::template)
    target_link_libraries(${test_name} PRIVATE vix::template)
  elseif (TARGET vix_template)
    target_link_libraries(${test_name} PRIVATE vix_template)
  elseif (TARGET template)
    target_link_libraries(${test_name} PRIVATE template)
  endif()

  target_compile_features(${test_name} PRIVATE cxx_std_20)

  if (BUILD_TESTING)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
endfunction()

function(vix_core_add_network_test test_name server_port)
  vix_core_add_test(${test_name}
    ${ARGN}
  )

  if (BUILD_TESTING)
    set_tests_properties(${test_name} PROPERTIES
      TIMEOUT 20
      RUN_SERIAL TRUE
      ENVIRONMENT "SERVER_PORT=${server_port}"
    )
  endif()
endfunction()

vix_core_add_test(core_smoke_test
  core_smoke_test.cpp
)

vix_core_add_test(runtime_test
  runtime/test_runtime.cpp
)

add_executable(runtime_bench
  runtime/bench_threadpool_vs_runtime.cpp
)

target_link_libraries(runtime_bench PRIVATE
  ${VIX_CORE_TEST_TARGET}
  ${VIX_CORE_TEST_UI_TARGET}
)

if (TARGET vix::template)
  target_link_libraries(runtime_bench PRIVATE vix::template)
elseif (TARGET vix_template)
  target_link_libraries(runtime_bench PRIVATE vix_template)
elseif (TARGET template)
  target_link_libraries(runtime_bench PRIVATE template)
endif()

target_compile_features(runtime_bench PRIVATE cxx_std_20)

vix_core_add_test(format_test
  format_test.cpp
)

vix_core_add_test(session_disconnect_tests
  session_disconnect_tests.cpp
)

vix_core_add_test(http_status_test
  http/status_test.cpp
)

vix_core_add_test(http_request_state_test
  http/request_state_test.cpp
)

vix_core_add_test(http_request_test
  http/request_test.cpp
)

vix_core_add_test(http_response_test
  http/response_test.cpp
)

vix_core_add_test(http_response_wrapper_test
  http/response_wrapper_test.cpp
)

vix_core_add_test(http_request_handler_helpers_test
  http/request_handler_helpers_test.cpp
)

vix_core_add_test(router_route_doc_test
  router/route_doc_test.cpp
)

vix_core_add_test(router_route_options_test
  router/route_options_test.cpp
)

vix_core_add_test(router_route_node_test
  router/route_node_test.cpp
)

vix_core_add_test(router_static_helpers_test
  router/router_static_helpers_test.cpp
)

vix_core_add_test(router_registration_test
  router/router_registration_test.cpp
)

vix_core_add_test(router_dispatch_test
  router/router_dispatch_test.cpp
)

vix_core_add_test(session_transport_test
  session/transport_test.cpp
)

vix_core_add_test(session_plain_transport_test
  session/plain_transport_test.cpp
)

vix_core_add_test(session_parsed_request_head_test
  session/parsed_request_head_test.cpp
)

vix_core_add_test(session_constructor_test
  session/session_constructor_test.cpp
)

vix_core_add_network_test(session_http_get_test 18081
  session/session_http_get_test.cpp
)

vix_core_add_network_test(session_http_body_test 18082
  session/session_http_body_test.cpp
)

vix_core_add_network_test(session_error_test 18083
  session/session_error_test.cpp
)

vix_core_add_network_test(session_keep_alive_test 18084
  session/session_keep_alive_test.cpp
)

vix_core_add_network_test(session_waf_test 18085
  session/session_waf_test.cpp
)

vix_core_add_network_test(session_tls_transport_test 18086
  session/tls_transport_test.cpp
)

vix_core_add_network_test(session_tls_session_test 18087
  session/tls_session_test.cpp
)

vix_core_add_network_test(server_tls_config_test 18090
  server/tls_config_test.cpp
)

vix_core_add_network_test(server_http_server_constructor_test 18091
  server/http_server_constructor_test.cpp
)

vix_core_add_network_test(server_http_server_router_test 18092
  server/http_server_router_test.cpp
)

vix_core_add_network_test(server_http_server_threading_test 18093
  server/http_server_threading_test.cpp
)

vix_core_add_network_test(server_http_server_shutdown_test 18094
  server/http_server_shutdown_test.cpp
)

vix_core_add_network_test(server_http_server_run_validation_test 18095
  server/http_server_run_validation_test.cpp
)

vix_core_add_test(runtime_budget_test
  runtime/budget_test.cpp
)

vix_core_add_test(runtime_task_test
  runtime/task_test.cpp
)

vix_core_add_test(runtime_mailbox_test
  runtime/mailbox_test.cpp
)

vix_core_add_test(runtime_run_queue_test
  runtime/run_queue_test.cpp
)

vix_core_add_test(runtime_supervisor_test
  runtime/supervisor_test.cpp
)

vix_core_add_test(runtime_worker_test
  runtime/worker_test.cpp
)

vix_core_add_test(runtime_scheduler_test
  runtime/scheduler_test.cpp
)

vix_core_add_test(runtime_config_test
  runtime/runtime_config_test.cpp
)

vix_core_add_test(runtime_lifecycle_test
  runtime/runtime_lifecycle_test.cpp
)

vix_core_add_test(executor_metrics_test
  executor/metrics_test.cpp
)

vix_core_add_test(executor_task_options_test
  executor/task_options_test.cpp
)

vix_core_add_test(executor_runtime_executor_constructor_test
  executor/runtime_executor_constructor_test.cpp
)

vix_core_add_test(executor_runtime_executor_lifecycle_test
  executor/runtime_executor_lifecycle_test.cpp
)

vix_core_add_test(executor_runtime_executor_submit_test
  executor/runtime_executor_submit_test.cpp
)

vix_core_add_test(executor_runtime_executor_post_test
  executor/runtime_executor_post_test.cpp
)

vix_core_add_test(executor_runtime_executor_metrics_test
  executor/runtime_executor_metrics_test.cpp
)

vix_core_add_test(executor_runtime_executor_shutdown_test
  executor/runtime_executor_shutdown_test.cpp
)

vix_core_add_test(config_defaults_test
  config/config_defaults_test.cpp
)

vix_core_add_test(config_env_test
  config/config_env_test.cpp
)

vix_core_add_test(config_set_get_test
  config/config_set_get_test.cpp
)

vix_core_add_test(config_typed_accessors_test
  config/config_typed_accessors_test.cpp
)

vix_core_add_test(config_database_test
  config/config_database_test.cpp
)

vix_core_add_test(config_waf_test
  config/config_waf_test.cpp
)

vix_core_add_test(config_tls_test
  config/config_tls_test.cpp
)

vix_core_add_test(config_logging_test
  config/config_logging_test.cpp
)

vix_core_add_test(config_reload_test
  config/config_reload_test.cpp
)

vix_core_add_test(config_copy_move_test
  config/config_copy_move_test.cpp
)

vix_core_add_test(app_constructor_test
  app/app_constructor_test.cpp
)

vix_core_add_test(app_config_test
  app/app_config_test.cpp
)

vix_core_add_test(app_dev_mode_test
  app/app_dev_mode_test.cpp
)

vix_core_add_test(app_templates_test
  app/app_templates_test.cpp
)

vix_core_add_test(app_routes_test
  app/app_routes_test.cpp
)

vix_core_add_test(app_group_test
  app/app_group_test.cpp
)

vix_core_add_test(app_middleware_test
  app/app_middleware_test.cpp
)

vix_core_add_test(app_protect_test
  app/app_protect_test.cpp
)

vix_core_add_test(app_static_dir_test
  app/app_static_dir_test.cpp
)

vix_core_add_test(app_static_hook_test
  app/app_static_hook_test.cpp
)

vix_core_add_network_test(app_lifecycle_test 19100
  app/app_lifecycle_test.cpp
)

vix_core_add_test(app_module_init_test
  app/app_module_init_test.cpp
)

