# Unit tests for test_engine.
#
# Built only when BUILD_UNIT_TESTS=ON (configured at the project root).
# Links against the test_engine shared library and GoogleTest.

set(SOURCES
        test_test_registry.cpp
)

add_executable(test_engine_tests ${SOURCES})

target_link_libraries(test_engine_tests
        PRIVATE
            test_engine
            GTest::gtest
            GTest::gtest_main
)

target_include_directories(test_engine_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/test_engine/src
        ${CMAKE_SOURCE_DIR}/runner_lib/include  # for <test_data.h>
)

set_target_properties(test_engine_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)

# On Windows, copy every runtime DLL the test exe depends on next to the binary
# so it can launch standalone (no PATH manipulation needed for gtest_discover_tests).
if(WIN32)
    add_custom_command(TARGET test_engine_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:test_engine_tests>
                $<TARGET_FILE_DIR:test_engine_tests>
            COMMAND_EXPAND_LISTS
    )
endif()

# Register individual TEST cases with CTest.
gtest_discover_tests(test_engine_tests
        DISCOVERY_TIMEOUT 30
        WORKING_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)
