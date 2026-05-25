cmake_minimum_required(VERSION 3.14)
project(test_data_format_solution)

set(CMAKE_CXX_STANDARD 17)

# No framework - allowedFrameworks=[] in config.json -> engine links runner_seq.
file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
