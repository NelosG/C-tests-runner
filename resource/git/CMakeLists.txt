# Git resource provider — clones/fetches repositories into a persistent cache

add_library(resource_git SHARED resource_git.cpp resource_git.h)

target_include_directories(resource_git PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}          # resource_git.h
        ${CMAKE_SOURCE_DIR}/resource/include # resource_provider.h, resource_context.h, register_resource_provider.h
        ${CMAKE_SOURCE_DIR}/server_core/src  # process_utils.h (header-only)
)

target_link_libraries(resource_git PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_definitions(resource_git PRIVATE RESOURCE_PROVIDER_EXPORTS)

if(WIN32)
    set_target_properties(resource_git PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    target_compile_options(resource_git PRIVATE -fPIC -fvisibility=default)
endif()

# Output to resource_providers/ subdirectory next to the executable
set_target_properties(resource_git PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/resource_providers
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/resource_providers
        PREFIX ""
)
