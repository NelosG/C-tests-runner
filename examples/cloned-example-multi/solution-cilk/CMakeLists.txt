cmake_minimum_required(VERSION 3.14)
project(parallel-scan-cilk)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)

# OpenCilk: requires clang with -fopencilk flag.
target_compile_options(student_solution PRIVATE -fopencilk)
target_link_options(student_solution PRIVATE -fopencilk)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
