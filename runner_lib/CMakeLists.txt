add_library(runner_lib STATIC src/runner.cpp)

target_include_directories(runner_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# No external dependencies - pure C++17 binary I/O

# Framework variants implement runner::setup(); each linked alone into a runner exe.
# Public API is in <runner.h> only (no per-variant headers).

# === OpenMP variant ===
find_package(OpenMP REQUIRED)

add_library(runner_omp STATIC omp/src/runner_omp.cpp)
target_link_libraries(runner_omp PUBLIC
    runner_lib
    parallel_lib
    OpenMP::OpenMP_CXX
)

# === ParlayLib variant (optional) ===
if(ENABLE_PARLAY)
    add_library(runner_parlay STATIC parlay/src/runner_parlay.cpp)
    target_link_libraries(runner_parlay PUBLIC runner_lib)
endif()

# === Cilk variant (compiler built-in, no extra deps) ===
add_library(runner_cilk STATIC cilk/src/runner_cilk.cpp)
target_link_libraries(runner_cilk PUBLIC runner_lib)

# === Sequential variant (no framework) ===
add_library(runner_seq STATIC seq/src/runner_seq.cpp)
target_link_libraries(runner_seq PUBLIC runner_lib)

# Static archives are linked into student runners - they MUST live in OUTPUT_DIR
# alongside the engine binaries (overrides global ARCHIVE_OUTPUT_DIRECTORY).
set(_runner_lib_targets runner_lib runner_omp runner_cilk runner_seq)
if(ENABLE_PARLAY)
    list(APPEND _runner_lib_targets runner_parlay)
endif()
foreach(_t ${_runner_lib_targets})
    set_target_properties(${_t} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})
endforeach()
