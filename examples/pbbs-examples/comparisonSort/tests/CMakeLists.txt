cmake_minimum_required(VERSION 3.14)
project(pbbs_comparison_sort_tests CXX)
set(CMAKE_CXX_STANDARD 17)

# `include/` carries the shared sort_test_common.h plus the vendored
# pbbsbench dataGen.h (with parlay::hash64 inlined, so the test plugins
# don't need parlay headers on their include path).
include_directories(include)

function(add_plugin name)
    add_library(${name} SHARED ${ARGN})
    target_link_libraries(${name} PRIVATE test_engine runner_lib)
    set_target_properties(${name} PROPERTIES PREFIX "")
endfunction()

add_plugin(plugin_correctness src/sort-test-correctness.cpp)
add_plugin(plugin_performance src/sort-test-performance.cpp)
