cmake_minimum_required(VERSION 3.14)
project(parallel-test-scan)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# SHARED for dynamic loading by server
add_library(replace-cmake-solution SHARED ${SOLUTION_SOURCES})

target_include_directories(replace-cmake-solution PUBLIC include)
target_link_libraries(replace-cmake-solution PUBLIC parallel_lib)

# Export all symbols on Windows
if(WIN32)
    set_target_properties(replace-cmake-solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
