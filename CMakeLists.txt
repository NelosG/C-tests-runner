# ==========================================================================
# C-tests-runner - Parallel computing test framework
#
# Structure:
#   parallel_lib/  - student-facing OpenMP wrappers with monitoring hooks
#   test_engine/   - lightweight plugin API (test, registry, result types)
#   server_core/   - server infrastructure (build, queue, runner, converters)
#   tests/         - example test plugins and student solutions
#   adapters/      - transport plugins (HTTP, RabbitMQ) loaded at runtime
#   tools/         - CLI runner, integration tests, mock orchestrators
#   server.cpp - adapter-based server mode
#   cli.cpp    - standalone CLI test runner
# ==========================================================================
cmake_minimum_required(VERSION 3.14)
project(c-test-runner C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform suffix: windows / linux / darwin
if(WIN32)
    set(PLATFORM_SUFFIX "windows")
else()
    set(PLATFORM_SUFFIX "linux")
endif()

# Output directories (.dll -> RUNTIME, .dll.a/.lib -> ARCHIVE, .so -> LIBRARY).
# By default, archives go to a throwaway dir inside the build tree - we don't
# want every adapter / plugin / FetchContent .dll.a / .a polluting OUTPUT_DIR.
# Targets whose archives are actually consumed (runner_*, test_engine,
# parallel_lib import-libs for student linking) override ARCHIVE_OUTPUT_DIRECTORY
# back to OUTPUT_DIR explicitly.
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}-${PLATFORM_SUFFIX})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/_archives)

# Create plugins directory
file(MAKE_DIRECTORY ${OUTPUT_DIR}/plugins)

# ============================================
# Dependencies via FetchContent
#
# nlohmann_json - JSON parsing/serialization (used everywhere)
# cpp-httplib   - HTTP server/client (pulled by adapters/http)
# AMQP-CPP      - AMQP 0-9-1 protocol (pulled by adapters/rabbit)
# libuv          - async I/O event loop (pulled by adapters/rabbit)
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

# ParlayLib (header-only) - optional, for parlay framework support
option(ENABLE_PARLAY "Enable ParlayLib framework support" ON)
if(ENABLE_PARLAY)
    FetchContent_Declare(
            parlaylib
            GIT_REPOSITORY https://github.com/cmuparlay/parlaylib.git
            GIT_TAG master
            EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(parlaylib)
endif()

# ============================================
# Subdirectories
# ============================================
add_subdirectory(parallel_lib)   # Student-facing OpenMP wrappers (shared lib)
add_subdirectory(test_engine)    # Lightweight plugin API (shared lib, no ext deps)
add_subdirectory(server_core)    # Server infrastructure (shared lib, links test_engine + deps)
add_subdirectory(runner_lib)    # Runner I/O + timing (static lib for student builds)
add_subdirectory(adapters)       # Transport adapters: HTTP, RabbitMQ (DLLs)
add_subdirectory(resource)       # Resource providers: git, local (DLLs)

# Dev-only artifacts - example test plugins, sample student solution, mock
# orchestrators. Not part of the production engine image; teacher/student
# bring their own. Default ON for local dev (so `cmake --build .` smoke-tests
# the whole tree); set BUILD_DEV_EXTRAS=OFF in production builds (Dockerfile).
option(BUILD_DEV_EXTRAS "Build example test plugins, sample solution, and mock orchestrators" ON)
if(BUILD_DEV_EXTRAS)
    add_subdirectory(tests)      # Example test plugins (DLLs) + sample student solution
    add_subdirectory(tools)      # Mock HTTP / RabbitMQ orchestrators (dev only)
endif()

# ============================================
# Unit tests - opt-in via -DBUILD_UNIT_TESTS=ON
# Each module owns its own test/ subdirectory; bin output goes to a separate
# unit_tests/ dir so test executables don't pollute OUTPUT_DIR.
# ============================================
option(BUILD_UNIT_TESTS "Build unit tests (GoogleTest)" OFF)
if(BUILD_UNIT_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            EXCLUDE_FROM_ALL
    )
    # gtest on Windows links the runtime statically by default; match our /MD.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    # Common output dir for unit test executables (kept out of OUTPUT_DIR).
    set(UNIT_TESTS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/unit_tests)
    file(MAKE_DIRECTORY ${UNIT_TESTS_OUTPUT_DIR})

    add_subdirectory(test_engine/test)
    add_subdirectory(server_core/test)
    add_subdirectory(runner_lib/test)
    add_subdirectory(parallel_lib/test)
endif()

# ============================================
# Server executable - loads adapter DLLs at runtime
# ============================================
add_executable(server server.cpp)
target_link_libraries(server PRIVATE server_core nlohmann_json::nlohmann_json)
target_include_directories(server PRIVATE
        ${CMAKE_SOURCE_DIR}/adapters/include
        ${CMAKE_SOURCE_DIR}/resource/include
        ${CMAKE_SOURCE_DIR}
)

add_executable(cli cli.cpp)
target_link_libraries(cli PRIVATE server_core nlohmann_json::nlohmann_json)
target_include_directories(cli PRIVATE
        ${CMAKE_SOURCE_DIR}/resource/include
        ${CMAKE_SOURCE_DIR}
)
# CLI loads resource_local at runtime - ensure it's built alongside cli
add_dependencies(cli resource_local)

# resource/include headers are part of the server_core public API (resource_manager.h uses them)
target_include_directories(server_core PUBLIC ${CMAKE_SOURCE_DIR}/resource/include)

# Create output directories for plugins
file(MAKE_DIRECTORY ${OUTPUT_DIR}/adapters)
file(MAKE_DIRECTORY ${OUTPUT_DIR}/resource_providers)

# Config is NOT copied/installed - drop config/ next to the exe at runtime
# (or set CONFIG_DIR env). In Docker the volume docker/config/ -> /opt/ctr/config/.

# Copy headers next to executables so BuildService/CMakeGenerator find them
file(COPY ${CMAKE_SOURCE_DIR}/test_engine/src/ DESTINATION ${OUTPUT_DIR}/include/test_engine
     FILES_MATCHING PATTERN "*.h")
file(COPY ${CMAKE_SOURCE_DIR}/parallel_lib/include/ DESTINATION ${OUTPUT_DIR}/include/parallel_lib)
file(COPY ${CMAKE_SOURCE_DIR}/parallel_lib/shadow/ DESTINATION ${OUTPUT_DIR}/include/shadow_omp)
file(COPY ${CMAKE_SOURCE_DIR}/runner_lib/include/ DESTINATION ${OUTPUT_DIR}/include/runner_lib)

# Per-framework runner sources mirrored to OUTPUT_DIR so local dev (cli/server
# run from output-Debug-*/) finds them at the same relative path as the
# installed Docker image (/opt/ctr/runner_lib/<variant>/src/runner_<variant>.cpp).
file(COPY ${CMAKE_SOURCE_DIR}/runner_lib/omp/src/    DESTINATION ${OUTPUT_DIR}/runner_lib/omp/src
     FILES_MATCHING PATTERN "*.cpp")
file(COPY ${CMAKE_SOURCE_DIR}/runner_lib/cilk/src/   DESTINATION ${OUTPUT_DIR}/runner_lib/cilk/src
     FILES_MATCHING PATTERN "*.cpp")
file(COPY ${CMAKE_SOURCE_DIR}/runner_lib/seq/src/    DESTINATION ${OUTPUT_DIR}/runner_lib/seq/src
     FILES_MATCHING PATTERN "*.cpp")
if(ENABLE_PARLAY)
    file(COPY ${CMAKE_SOURCE_DIR}/runner_lib/parlay/src/ DESTINATION ${OUTPUT_DIR}/runner_lib/parlay/src
         FILES_MATCHING PATTERN "*.cpp")
endif()
if(ENABLE_PARLAY)
    file(COPY ${parlaylib_SOURCE_DIR}/include/parlay DESTINATION ${OUTPUT_DIR}/include)
endif()

# Static CMake helpers + templates used by BuildService at runtime.
file(COPY ${CMAKE_SOURCE_DIR}/cmake/ DESTINATION ${OUTPUT_DIR}/cmake
     FILES_MATCHING
         PATTERN "*.cmake"
         PATTERN "*.cmake.in")

if(UNIX)
    set_target_properties(server cli PROPERTIES
            BUILD_RPATH "${OUTPUT_DIR}"
            INSTALL_RPATH "$ORIGIN"
    )
endif()

# Copy MinGW runtime DLLs so runner child processes can find them
if(WIN32 AND MINGW)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    foreach(dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libgomp-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${dll}")
            file(COPY "${MINGW_BIN_DIR}/${dll}" DESTINATION "${OUTPUT_DIR}")
        endif()
    endforeach()
endif()

# ============================================
# Install rules
# ============================================
install(TARGETS server cli
        RUNTIME DESTINATION .
)
install(TARGETS http_adapter rabbit_adapter
        LIBRARY DESTINATION adapters
        RUNTIME DESTINATION adapters
)
install(TARGETS resource_git resource_local
        LIBRARY DESTINATION resource_providers
        RUNTIME DESTINATION resource_providers
)
install(TARGETS test_engine server_core parallel_lib
        LIBRARY DESTINATION .
        RUNTIME DESTINATION .
        ARCHIVE DESTINATION .
)
install(DIRECTORY test_engine/src/
        DESTINATION include/test_engine
        FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY server_core/src/
        DESTINATION include/server_core
        FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY parallel_lib/include/ DESTINATION include/parallel_lib)
install(DIRECTORY parallel_lib/shadow/ DESTINATION include/shadow_omp FILES_MATCHING PATTERN "*.h")
# Only runner_lib (base, framework-agnostic) is pre-built and installed as an
# archive. Framework variants (runner_omp/parlay/cilk/seq) are compiled per-job
# at student-build time with the same compiler that builds the student's
# solution (see CMakeGenerator::FRAMEWORK_BLOCK). We ship their sources here.
install(TARGETS runner_lib ARCHIVE DESTINATION .)

# Per-framework runner sources (compiled per-job at student-build time)
install(DIRECTORY runner_lib/omp/src/    DESTINATION runner_lib/omp/src    FILES_MATCHING PATTERN "*.cpp")
install(DIRECTORY runner_lib/cilk/src/   DESTINATION runner_lib/cilk/src   FILES_MATCHING PATTERN "*.cpp")
install(DIRECTORY runner_lib/seq/src/    DESTINATION runner_lib/seq/src    FILES_MATCHING PATTERN "*.cpp")
if(ENABLE_PARLAY)
    install(DIRECTORY runner_lib/parlay/src/ DESTINATION runner_lib/parlay/src FILES_MATCHING PATTERN "*.cpp")
endif()

install(DIRECTORY runner_lib/include/ DESTINATION include/runner_lib FILES_MATCHING PATTERN "*.h")
if(ENABLE_PARLAY)
    install(DIRECTORY ${parlaylib_SOURCE_DIR}/include/parlay DESTINATION include)
endif()

# CMake helpers and templates read by BuildService / CMakeGenerator at runtime
# (runner_wrapper.cmake.in, test_plugin_wrapper.cmake.in, block_network.cmake).
install(DIRECTORY cmake/ DESTINATION cmake
        FILES_MATCHING
            PATTERN "*.cmake"
            PATTERN "*.cmake.in")
