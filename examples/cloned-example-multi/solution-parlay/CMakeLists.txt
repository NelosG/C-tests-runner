cmake_minimum_required(VERSION 3.14)
project(parallel-scan-parlay)

set(CMAKE_CXX_STANDARD 17)

# Framework marker: parlay
# parlaylib is header-only; the engine prepends parlay headers to the include
# path (PARLAY_HEADERS_PATH) when the parlay framework is detected. The
# `parlay` token below is what the engine's FrameworkDetector looks for.
set(USING_FRAMEWORK parlay)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
