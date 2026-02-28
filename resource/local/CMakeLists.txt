# Local filesystem resource provider — resolves paths with base-dir support

add_library(resource_local SHARED resource_local.cpp resource_local.h)

target_include_directories(resource_local PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}          # resource_local.h
        ${CMAKE_SOURCE_DIR}/resource/include # resource_provider.h, resource_context.h, register_resource_provider.h
)

target_link_libraries(resource_local PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_definitions(resource_local PRIVATE RESOURCE_PROVIDER_EXPORTS)

if(WIN32)
    set_target_properties(resource_local PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    target_compile_options(resource_local PRIVATE -fPIC -fvisibility=default)
endif()

# Output to resource_providers/ subdirectory next to the executable
set_target_properties(resource_local PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/resource_providers
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/resource_providers
        PREFIX ""
)
