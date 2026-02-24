# ==========================================================================
# C-tests-runner — Parallel computing test framework
#
# Structure:
#   parallel_lib/  — student-facing OpenMP wrappers with monitoring hooks
#   test_engine/   — core shared library (build, load, execute, queue)
#   tests/         — example test plugins and student solutions
#   adapters/      — transport plugins (HTTP, RabbitMQ) loaded at runtime
#   tools/         — CLI runner, integration tests, mock orchestrators
#   server_main.cpp — entry point (server mode or CLI mode)
# ==========================================================================
cmake_minimum_required(VERSION 3.14)
project(c-test-runner C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories (.dll → RUNTIME, .dll.a/.lib → ARCHIVE, .so → LIBRARY)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE})

# Create plugins directory
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/plugins)

# ============================================
# Dependencies via FetchContent
#
# nlohmann_json — JSON parsing/serialization (used everywhere)
# cpp-httplib   — HTTP server/client (pulled by adapters/http)
# AMQP-CPP      — AMQP 0-9-1 protocol (pulled by adapters/rabbit)
# libuv          — async I/O event loop (pulled by adapters/rabbit)
# ============================================
include(FetchContent)

# Only nlohmann_json is declared at root level; other deps are
# declared in their respective adapter CMakeLists.txt files.
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(nlohmann_json)

# ============================================
# Subdirectories
# ============================================
add_subdirectory(parallel_lib)   # Student-facing OpenMP wrappers (shared lib)
add_subdirectory(test_engine)    # Core test framework (shared lib)
add_subdirectory(tests)          # Example test plugins (DLLs)
add_subdirectory(adapters)       # Transport adapters: HTTP, RabbitMQ (DLLs)
add_subdirectory(tools)          # CLI tools and mock orchestrators

# ============================================
# Server executable — loads adapter DLLs at runtime
# ============================================
add_executable(server server_main.cpp)
target_link_libraries(server PRIVATE test_engine nlohmann_json::nlohmann_json)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/adapters/include)

# Create adapters output directory
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/adapters)

if(UNIX)
    set_target_properties(server PROPERTIES
            BUILD_RPATH "${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}"
            INSTALL_RPATH "$ORIGIN/../lib"
    )
endif()

# ============================================
# Install rules
# ============================================
install(TARGETS server
        RUNTIME DESTINATION bin
)
install(TARGETS test_engine parallel_lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
)
install(DIRECTORY test_engine/src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY parallel_lib/include/par
        DESTINATION include
)
