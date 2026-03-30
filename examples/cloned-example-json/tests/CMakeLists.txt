cmake_minimum_required(VERSION 3.14)
project(sum_tests CXX)
set(CMAKE_CXX_STANDARD 17)

# Empty: this assignment uses only JSON scenarios in tests/cases/.
# No C++ plugins needed. The wrapper still calls add_subdirectory(),
# CMake just produces no plugin targets.
