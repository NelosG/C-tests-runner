cmake_minimum_required(VERSION 3.14)
project(c-test-runner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Выходные директории
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE})


# Создаем директорию для плагинов
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/plugins)

add_subdirectory(solution)
add_subdirectory(test_engine)
add_subdirectory(tests)

#file(GLOB_RECURSE EXTENSION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/extensions/*.cpp)

# Исполняемый файл - теперь НЕ линкуется с тестами напрямую
# Тесты загружаются динамически из папки plugins/
add_executable(run main.cpp #[[ ${EXTENSION_SOURCES}]])
target_link_libraries(run PRIVATE test_engine  #[[test_extensions]] #[[student_solution]])

# Для Linux - нужно добавить rpath для поиска libtest_engine.so
if(UNIX)
    set_target_properties(run PROPERTIES
        BUILD_RPATH "${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}"
        INSTALL_RPATH "${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}"
    )
endif()
