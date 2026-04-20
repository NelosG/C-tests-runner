cmake_minimum_required(VERSION 3.14)
project(server_core CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP REQUIRED)

# Server infrastructure - build service, job queue, runner, converters, etc.
set(CORE_SOURCES
        src/adapter_manager.cpp
        src/assignment_config.cpp
        src/build_service.cpp
        src/cmake_generator.cpp
        src/cmake_validator.cpp
        src/cpu_isolator.cpp
        src/dll_plugin_helpers.cpp
        src/framework_detector.cpp
        src/job_queue.cpp
        src/json_scenario_loader.cpp
        src/numa_utils.cpp
        src/pipeline.cpp
        src/plugin_loader.cpp
        src/sandbox_launcher.cpp
        src/sandbox_test_executor.cpp
        src/test_result_converter.cpp
        src/test_runner_service.cpp
        src/test_scenario_result_converter.cpp
)

set(CORE_HEADERS
        src/adapter_manager.h
        src/api_types.h
        src/assignment_config.h
        src/build_service.h
        src/cmake_generator.h
        src/cmake_validator.h
        src/config_utils.h
        src/cpu_isolator.h
        src/dll_plugin_helpers.h
        src/dll_utils.h
        src/framework_detector.h
        src/job_queue.h
        src/json_scenario_loader.h
        src/numa_utils.h
        src/path_sanitizer.h
        src/pipeline.h
        src/plugin_loader.h
        src/process_utils.h

        src/resource_manager.h
        src/sandbox_launcher.h
        src/sandbox_test_executor.h
        src/test_result_converter.h
        src/test_runner_service.h
        src/test_scenario_result_converter.h
        src/thread_counts.h
        src/time_utils.h
)

add_library(server_core SHARED ${CORE_SOURCES} ${CORE_HEADERS})

if(WIN32)
    set_target_properties(server_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_link_libraries(server_core PRIVATE psapi)
endif()

if(UNIX)
    target_compile_options(server_core PRIVATE -fvisibility=default)
endif()

target_include_directories(server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# adapter_manager.{h,cpp} live in server_core/src but pull adapter_api.h /
# adapter_context.h / adapter_status.h / transport_adapter.h from adapters/include.
target_include_directories(server_core PRIVATE ${CMAKE_SOURCE_DIR}/adapters/include)

# Pass build type to sandbox builds so they match our .a files
target_compile_definitions(server_core PRIVATE CTR_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

target_link_libraries(server_core PUBLIC
        test_engine
        nlohmann_json::nlohmann_json
        parallel_lib
        OpenMP::OpenMP_CXX
        ${CMAKE_DL_LIBS}
)

if(UNIX)
    option(USE_LIBNUMA "Use libnuma for NUMA operations (default ON)" ON)
    if(USE_LIBNUMA)
        find_library(NUMA_LIBRARY numa)
        if(NUMA_LIBRARY)
            target_link_libraries(server_core PRIVATE ${NUMA_LIBRARY})
            target_compile_definitions(server_core PRIVATE NUMA_USE_LIBNUMA)
            message(STATUS "NUMA: using libnuma")
        else()
            message(WARNING "NUMA: libnuma not found, falling back to syscall-based implementation")
        endif()
    endif()
    find_package(Threads REQUIRED)
    target_link_libraries(server_core PUBLIC Threads::Threads)
endif()
