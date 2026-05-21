cmake_minimum_required(VERSION 3.14)
project(parallel-quicksort-omp-raw)

set(CMAKE_CXX_STANDARD 17)

# Pure OpenMP - no parallel_lib wrapper. Allowed because tests/config.json
# lists "OpenMP" in allowedPackages, which disables the engine's shadow <omp.h>.
find_package(OpenMP REQUIRED)

file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)
target_link_libraries(student_solution PUBLIC OpenMP::OpenMP_CXX)

if(WIN32)
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
