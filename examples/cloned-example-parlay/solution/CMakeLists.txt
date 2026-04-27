cmake_minimum_required(VERSION 3.14)
project(parallel-scan-parlay)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# parlaylib is header-only. The engine adds parlay headers to the include path
# via PARLAY_HEADERS_PATH at the runner build wrapper level, so no FetchContent
# or find_package call is needed here.
add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
