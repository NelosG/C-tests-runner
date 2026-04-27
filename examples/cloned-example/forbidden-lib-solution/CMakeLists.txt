cmake_minimum_required(VERSION 3.14)
project(parallel-scan-parlay)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    parlaylib
    GIT_REPOSITORY https://github.com/cmuparlay/parlaylib.git
    GIT_TAG v2.4.1
)
FetchContent_MakeAvailable(parlaylib)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)
target_link_libraries(student_solution PUBLIC parlay)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
