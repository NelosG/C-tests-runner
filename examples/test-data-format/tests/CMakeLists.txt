cmake_minimum_required(VERSION 3.14)
project(test_data_format_tests CXX)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

function(add_plugin name)
    add_library(${name} SHARED ${ARGN})
    target_link_libraries(${name} PRIVATE test_engine runner_lib)
    set_target_properties(${name} PROPERTIES PREFIX "")
endfunction()

add_plugin(plugin_format_roundtrip src/format-roundtrip-correctness.cpp)
