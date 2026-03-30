cmake_minimum_required(VERSION 3.14)
project(sum_solution_seq CXX)
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
