# Unit tests for runner_lib - focused on the TestData header-only binary
# serialisation that the runner, JSON scenarios, and teacher plugins share.

set(SOURCES
        test_runner.cpp
        test_test_data.cpp
)

add_executable(runner_lib_tests ${SOURCES})

# runner_lib (static) brings runner.cpp; test_data.h is header-only and
# carried in PUBLIC include dir.
target_link_libraries(runner_lib_tests
        PRIVATE
            runner_lib
            GTest::gtest
            GTest::gtest_main
)

target_include_directories(runner_lib_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/runner_lib/include
        ${CMAKE_SOURCE_DIR}/server_core/test  # for test_temp_dir.h
)

set_target_properties(runner_lib_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)

# NOTE: no TARGET_RUNTIME_DLLS copy needed - runner_lib_tests links only against
# the header-only TestData API and statically-linked GoogleTest, so the exe is
# self-contained. (If we add a dependency on a SHARED library later, copy DLLs
# next to the test exe the way test_engine/test does.)

gtest_discover_tests(runner_lib_tests
        DISCOVERY_TIMEOUT 30
        WORKING_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)
