# AMQP-CPP + libuv — async RabbitMQ client
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)  # AMQP-CPP uses cmake_minimum_required(2.8.12)

include(FetchContent)

set(AMQP-CPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(AMQP-CPP_LINUX_TCP OFF CACHE BOOL "" FORCE)
set(AMQP-CPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        amqpcpp
        GIT_REPOSITORY https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git
        GIT_TAG v4.3.27
)

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG v1.48.0
)

FetchContent_MakeAvailable(amqpcpp libuv)

# RabbitMQ adapter — async AMQP-CPP + libuv, loaded at runtime
add_library(rabbit_adapter SHARED rabbit_adapter.cpp)

target_include_directories(rabbit_adapter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}          # rabbit_adapter.h, uv_amqp_handler.h, control_type.h
        ${CMAKE_SOURCE_DIR}/adapters/include # transport_adapter.h, adapter_api.h
)

target_link_libraries(rabbit_adapter PRIVATE
        test_engine
        amqpcpp
        uv_a
        nlohmann_json::nlohmann_json
)

# libuv on Windows needs extra system libs
if(WIN32)
    target_link_libraries(rabbit_adapter PRIVATE ws2_32 iphlpapi userenv)
endif()

target_compile_definitions(rabbit_adapter PRIVATE ADAPTER_EXPORTS)

set_target_properties(rabbit_adapter PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/adapters
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/adapters
        PREFIX ""
)
