# ============================================
# Integration test — full pipeline without HTTP
# ============================================
add_executable(test_server_pipeline src/test_server_pipeline.cpp)
target_link_libraries(test_server_pipeline PRIVATE server_core parallel_lib)

# ============================================
# RabbitMQ client mock — orchestrator stub for RabbitMQ adapter
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
# HTTP client mock — orchestrator stub for HTTP adapter
# ============================================
add_executable(http_client_mock src/http_client_mock.cpp)
target_include_directories(http_client_mock PRIVATE
        ${CMAKE_SOURCE_DIR}/server_core/src   # for time_utils.h
)
target_link_libraries(http_client_mock PRIVATE httplib::httplib nlohmann_json::nlohmann_json)

# ============================================
# RPATH for Linux
# ============================================
if(UNIX)
    set_target_properties(run test_server_pipeline PROPERTIES
            BUILD_RPATH "${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}"
            INSTALL_RPATH "$ORIGIN/../lib"
    )
endif()
