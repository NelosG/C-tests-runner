# cpp-httplib — HTTP server/client
include(FetchContent)

# Disable optional compression libs — not needed for REST API,
# and prevents zlib DLLs from leaking into the adapters directory.
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        cpp-httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.18.3
)
FetchContent_MakeAvailable(cpp-httplib)

# HTTP adapter — shared library loaded at runtime
add_library(http_adapter SHARED http_adapter.cpp)

target_include_directories(http_adapter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}          # http_adapter.h
        ${CMAKE_SOURCE_DIR}/adapters/include # transport_adapter.h, adapter_api.h
)

target_link_libraries(http_adapter PRIVATE
        test_engine
        httplib::httplib
        nlohmann_json::nlohmann_json
)

target_compile_definitions(http_adapter PRIVATE ADAPTER_EXPORTS)

# Output to adapters/ subdirectory next to the executable
set_target_properties(http_adapter PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/adapters
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/adapters
        PREFIX ""
)
