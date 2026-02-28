cmake_minimum_required(VERSION 3.14)
project(test_engine CXX)
set(CMAKE_CXX_STANDARD 17)

# Lightweight plugin API — no external dependencies (pure C++17)
set(ENGINE_SOURCES
        src/test_registry.cpp
        src/test_scenario_extension.cpp
)

set(ENGINE_HEADERS
        src/plugin_export.h
        src/register_test.h
        src/test.h
        src/test_registry.h
        src/test_result.h
        src/test_scenario_extension.h
        src/test_scenario_result.h
)

# SHARED is required — TestRegistry singleton + thread_local must be shared across DLLs
add_library(test_engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

if(WIN32)
    set_target_properties(test_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(UNIX)
    target_compile_options(test_engine PRIVATE -fvisibility=default)
endif()

target_include_directories(test_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
