# Unit tests for parallel_lib monitor - counter / mode / context bookkeeping.
# We don't exercise OpenMP-parallel paths here; that's the engine's job.

set(SOURCES
        test_monitor.cpp
)

add_executable(parallel_lib_tests ${SOURCES})

target_link_libraries(parallel_lib_tests
        PRIVATE
            parallel_lib
            GTest::gtest
            GTest::gtest_main
)

target_include_directories(parallel_lib_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/parallel_lib/include
)

set_target_properties(parallel_lib_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)

if(WIN32)
    add_custom_command(TARGET parallel_lib_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:parallel_lib_tests>
                $<TARGET_FILE_DIR:parallel_lib_tests>
            COMMAND_EXPAND_LISTS
    )
endif()

gtest_discover_tests(parallel_lib_tests
        DISCOVERY_TIMEOUT 30
        WORKING_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR}
)
