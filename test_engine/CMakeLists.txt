cmake_minimum_required(VERSION 3.14)
project(test_engine CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP REQUIRED)

# Explicit source list (no GLOB_RECURSE — new files must be added here)
set(ENGINE_SOURCES
        src/build_service.cpp
        src/cmake_generator.cpp
        src/job_queue.cpp
        src/numa_utils.cpp
        src/plugin_loader.cpp
        src/test_engine.cpp
        src/test_registry.cpp
        src/test_result_converter.cpp
        src/test_runner_service.cpp
        src/test_scenario_extension.cpp
        src/test_scenario_result_converter.cpp
)

set(ENGINE_HEADERS
        src/build_service.h
        src/cmake_generator.h
        src/config_utils.h
        src/dll_utils.h
        src/job_queue.h
        src/numa_utils.h
        src/plugin_export.h
        src/plugin_loader.h
        src/process_utils.h
        src/project_utils.h
        src/register_test.h
        src/test.h
        src/test_engine.h
        src/test_registry.h
        src/test_result.h
        src/test_result_converter.h
        src/test_runner_service.h
        src/test_scenario_extension.h
        src/test_scenario_result.h
        src/test_scenario_result_converter.h
        src/thread_counts.h
        src/time_utils.h
)

# Build test_engine as SHARED library
add_library(test_engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Export all symbols on Windows
if(WIN32)
    set_target_properties(test_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Symbol visibility on Linux
if(UNIX)
    target_compile_options(test_engine PRIVATE -fvisibility=default)
endif()

target_include_directories(test_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(test_engine PUBLIC
        nlohmann_json::nlohmann_json
        parallel_lib
        OpenMP::OpenMP_CXX
        ${CMAKE_DL_LIBS}
)

# pthreads for job_queue on Linux
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(test_engine PUBLIC Threads::Threads)
endif()
