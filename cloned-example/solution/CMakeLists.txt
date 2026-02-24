cmake_minimum_required(VERSION 3.14)
project(parallel-test-scan)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# SHARED for dynamic loading by server
add_library(student_solution SHARED ${SOLUTION_SOURCES})

target_include_directories(student_solution PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(student_solution PUBLIC parallel_lib)

# Export all symbols on Windows
if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
