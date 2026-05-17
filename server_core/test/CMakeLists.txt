# Unit tests for server_core.
#
# Built only when BUILD_UNIT_TESTS=ON. Links against the server_core shared
# library (pulls in test_engine + nlohmann_json + parallel_lib + OpenMP).

set(SOURCES
        test_api_types.cpp
        test_assignment_config.cpp
        test_cmake_generator.cpp
        test_cmake_validator.cpp
        test_config_utils.cpp
        test_dll_plugin_helpers.cpp
        test_framework_detector.cpp
        test_job_queue.cpp
        test_json_scenario_loader.cpp
        test_path_sanitizer.cpp
        test_process_utils.cpp
        test_request_helpers.cpp
        test_sandbox_summary.cpp
        test_test_result_converter.cpp
        test_test_scenario_result_converter.cpp
        test_thread_counts.cpp
        test_time_utils.cpp
)

add_executable(server_core_tests ${SOURCES})

target_link_libraries(server_core_tests
        PRIVATE
            server_core
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gtest_main
)

target_include_directories(server_core_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/server_core/src
        ${CMAKE_SOURCE_DIR}/test_engine/src
        ${CMAKE_SOURCE_DIR}/runner_lib/include
)

set_target_properties(server_core_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)

if(WIN32)
    add_custom_command(TARGET server_core_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:server_core_tests>
                $<TARGET_FILE_DIR:server_core_tests>
            COMMAND_EXPAND_LISTS
    )
endif()

gtest_discover_tests(server_core_tests
        DISCOVERY_TIMEOUT 30
        WORKING_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)
