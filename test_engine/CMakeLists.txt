cmake_minimum_required(VERSION 3.14)
project(test_engine CXX)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(CURL REQUIRED)
find_package(OpenMP REQUIRED)

file(GLOB_RECURSE ENGINE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Собираем test_engine как SHARED библиотеку для использования плагинами
add_library(test_engine SHARED ${ENGINE_SOURCES})

# Экспортируем все символы (для Windows)
if(WIN32)
    set_target_properties(test_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Для Linux - видимость символов
if(UNIX)
    target_compile_options(test_engine PRIVATE -fvisibility=default)
endif()

target_include_directories(test_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(test_engine PUBLIC
        nlohmann_json::nlohmann_json
        CURL::libcurl
        OpenMP::OpenMP_CXX
        ${CMAKE_DL_LIBS}  # Для dlopen/dlclose на Linux
)

