add_library(runner_lib STATIC src/runner.cpp)

target_include_directories(runner_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# No external dependencies - pure C++17 binary I/O

# Framework variants (runner_omp/parlay/cilk/seq) are **NOT** pre-built at
# engine build time. Their source files are installed and compiled per-job
# at student-build time with the same compiler that compiles the student's
# solution. This is the only way Cilk can have a `cilk_for` warmup
# (cilk_for is an OpenCilk-clang keyword), and it gives architectural
# symmetry across frameworks: each variant's source is compiled by exactly
# the compiler that understands its parallelism dialect.
#
# See `server_core/src/cmake_generator.cpp` (FRAMEWORK_BLOCK) for the
# wrapper CMake that does this at student-build time, and the root
# `CMakeLists.txt` install rules for the source-install destinations.

# librunner_lib.a is still pre-built - it's pure C++17, framework-agnostic.
set_target_properties(runner_lib PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})
