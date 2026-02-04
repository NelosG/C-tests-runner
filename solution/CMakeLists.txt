cmake_minimum_required(VERSION 3.14)
project(student_solution CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP REQUIRED)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution ${SOLUTION_SOURCES})

target_include_directories(student_solution PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(student_solution PUBLIC OpenMP::OpenMP_CXX)
