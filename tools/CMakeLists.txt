# ============================================
# RabbitMQ client mock - orchestrator stub for RabbitMQ adapter
# ============================================
add_executable(rabbit_client_mock src/rabbit_client_mock.cpp)
target_include_directories(rabbit_client_mock PRIVATE
        ${CMAKE_SOURCE_DIR}/adapters/rabbit
        ${CMAKE_SOURCE_DIR}/adapters/include
        ${CMAKE_SOURCE_DIR}/server_core/src   # for time_utils.h
)
target_link_libraries(rabbit_client_mock PRIVATE amqpcpp uv_a nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(rabbit_client_mock PRIVATE ws2_32 iphlpapi userenv)
endif()

# ============================================
# HTTP client mock - orchestrator stub for HTTP adapter
# ============================================
add_executable(http_client_mock src/http_client_mock.cpp)
target_include_directories(http_client_mock PRIVATE
        ${CMAKE_SOURCE_DIR}/server_core/src   # for time_utils.h
)
target_link_libraries(http_client_mock PRIVATE httplib::httplib nlohmann_json::nlohmann_json)

# Mocks are dev-only utilities (orchestrator stubs for adapter testing).
# They live next to the engine binaries but in a tools/ subfolder so they
# don't clutter OUTPUT_DIR's runtime layout.
set_target_properties(rabbit_client_mock http_client_mock PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/tools)

if(UNIX)
    set_target_properties(rabbit_client_mock http_client_mock PROPERTIES
            BUILD_RPATH "${OUTPUT_DIR}"
            INSTALL_RPATH "$ORIGIN/.."
    )
endif()
