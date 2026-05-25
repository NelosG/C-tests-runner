# Unit tests for server_core.
#
# Built only when BUILD_UNIT_TESTS=ON. Links against the server_core shared
# library (pulls in test_engine + nlohmann_json + parallel_lib + OpenMP).

set(SOURCES
        test_api_types.cpp
        test_assignment_config.cpp
        test_build_test_result.cpp
        test_cmake_generator.cpp
        test_cmake_validator.cpp
        test_config_utils.cpp
        test_dll_plugin_helpers.cpp
        test_dll_plugin_helpers_extras.cpp
        test_framework_detector.cpp
        test_job_queue.cpp
        test_job_queue_extras.cpp
        test_json_scenario_loader.cpp
        test_json_scenario_loader_extras.cpp
        test_path_sanitizer.cpp
        test_pipeline_helpers.cpp
        test_plugin_loader.cpp
        test_process_utils.cpp
        test_request_helpers.cpp
        test_resource_manager.cpp
        test_sandbox_summary.cpp
        test_test_result_converter.cpp
        test_test_scenario_result_converter.cpp
        test_thread_counts.cpp
        test_time_utils.cpp
)

# Fixture shared library for plugin_loader tests.
# Just needs to be loadable by dlopen/LoadLibrary - no exported symbols required.
add_library(dummy_test_plugin SHARED fixtures/dummy_plugin.cpp)
set_target_properties(dummy_test_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
        PREFIX ""           # avoid lib prefix on linux for symmetry with windows
)

# Fake resource provider fixture - implements the full ResourceProvider
# contract with behaviour driven by config flags (reject / ctor_null / throw_start).
# Lives in its own subdirectory so ResourceManager's scan_plugin_dir picks up
# exactly one provider during the test run.
set(FAKE_PROVIDER_DIR ${UNIT_TESTS_OUTPUT_DIR}/resource_providers)
file(MAKE_DIRECTORY ${FAKE_PROVIDER_DIR})
add_library(resource_fake SHARED fixtures/fake_resource_provider.cpp)
target_include_directories(resource_fake PRIVATE
        ${CMAKE_SOURCE_DIR}/resource/include
        ${CMAKE_SOURCE_DIR}/server_core/src
)
set_target_properties(resource_fake PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${FAKE_PROVIDER_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${FAKE_PROVIDER_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${FAKE_PROVIDER_DIR}
        PREFIX ""             # match production resource_* layout (no lib prefix)
        OUTPUT_NAME "resource_fake"
)
target_link_libraries(resource_fake PRIVATE nlohmann_json::nlohmann_json)

add_executable(server_core_tests ${SOURCES})
add_dependencies(server_core_tests dummy_test_plugin resource_fake)
# Inject fixture paths so tests can locate artifacts without env vars / search paths.
target_compile_definitions(server_core_tests PRIVATE
        CTR_DUMMY_PLUGIN_PATH="$<TARGET_FILE:dummy_test_plugin>"
        CTR_FAKE_PROVIDER_DIR="${FAKE_PROVIDER_DIR}"
)

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
