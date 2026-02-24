cmake_minimum_required(VERSION 3.14)
project(parallel_lib CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenMP REQUIRED)

add_library(parallel_lib SHARED src/monitor.cpp src/memory_guard.cpp)

target_include_directories(parallel_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(parallel_lib PUBLIC OpenMP::OpenMP_CXX)

if(WIN32)
    set_target_properties(parallel_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(UNIX)
    target_compile_options(parallel_lib PRIVATE -fvisibility=default)
    find_package(Threads REQUIRED)
    target_link_libraries(parallel_lib PUBLIC Threads::Threads)
endif()
