cmake_minimum_required(VERSION 3.14)
project(quick_sort_tests CXX)
set(CMAKE_CXX_STANDARD 17)

# `test_engine` and `runner_lib` are provided by the engine wrapper as
# imported targets - they carry the right include dirs / compile options.
include_directories(include)

function(add_plugin name)
    add_library(${name} SHARED ${ARGN})
    target_link_libraries(${name} PRIVATE test_engine runner_lib)
    set_target_properties(${name} PROPERTIES PREFIX "")
endfunction()

add_plugin(plugin_correctness src/test_correctness.cpp)
add_plugin(plugin_edge_cases src/test_edge_cases.cpp)
add_plugin(plugin_stability_scalability src/test_stability.cpp src/test_scalability.cpp)
add_plugin(plugin_parallel_verify src/test_parallel_verify.cpp)
