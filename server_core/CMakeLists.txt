cmake_minimum_required(VERSION 3.14)
project(server_core CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP REQUIRED)

# Server infrastructure — build service, job queue, runner, converters, etc.
set(CORE_SOURCES
        src/build_service.cpp
        src/cmake_generator.cpp
        src/job_queue.cpp
        src/numa_utils.cpp
        src/plugin_loader.cpp
        src/test_engine.cpp
        src/test_result_converter.cpp
        src/test_runner_service.cpp
        src/test_scenario_result_converter.cpp
)

set(CORE_HEADERS
        src/api_types.h
        src/build_service.h
        src/cmake_generator.h
        src/config_utils.h
        src/dll_utils.h
        src/job_queue.h
        src/numa_utils.h
        src/plugin_loader.h
        src/process_utils.h
        src/project_utils.h
        src/resource_manager.h
        src/test_engine.h
        src/test_result_converter.h
        src/test_runner_service.h
        src/test_scenario_result_converter.h
        src/thread_counts.h
        src/time_utils.h
)

add_library(server_core SHARED ${CORE_SOURCES} ${CORE_HEADERS})

if(WIN32)
    set_target_properties(server_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(UNIX)
    target_compile_options(server_core PRIVATE -fvisibility=default)
endif()

target_include_directories(server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(server_core PUBLIC
        test_engine
        nlohmann_json::nlohmann_json
        parallel_lib
        OpenMP::OpenMP_CXX
        ${CMAKE_DL_LIBS}
)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(server_core PUBLIC Threads::Threads)
endif()
