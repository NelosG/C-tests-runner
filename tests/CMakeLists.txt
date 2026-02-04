cmake_minimum_required(VERSION 3.14)
project(extensions CXX)
set(CMAKE_CXX_STANDARD 17)

# ============================================
# Общие настройки для всех плагинов
# ============================================
if(WIN32)
    add_compile_definitions(PLUGIN_EXPORTS)
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=default")
endif()

# Директория для плагинов
set(PLUGIN_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/plugins)
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

# ============================================
# Плагин 1: test_correctness (тесты корректности)
# ============================================
add_library(plugin_correctness SHARED
    extensions/test_correctness.cpp
        extensions/register-all.cpp
)

target_link_libraries(plugin_correctness 
    PUBLIC test_engine 
    PUBLIC student_solution
)

set_target_properties(plugin_correctness PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    PREFIX ""  # Убираем lib префикс на Linux
)

# ============================================
# Плагин 2: test_perf (тесты производительности)
# ============================================
add_library(plugin_perf SHARED
    extensions/test_perf.cpp
        extensions/register-all.cpp
)

target_link_libraries(plugin_perf 
    PUBLIC test_engine 
    PUBLIC student_solution
)

set_target_properties(plugin_perf PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    PREFIX ""  # Убираем lib префикс на Linux
)

# ============================================
# Опционально: старая единая библиотека (для совместимости)
# ============================================
# file(GLOB_RECURSE EXTENSION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/extensions/*.cpp)
# add_library(test_extensions SHARED ${EXTENSION_SOURCES})
# target_link_libraries(test_extensions PUBLIC test_engine student_solution)
