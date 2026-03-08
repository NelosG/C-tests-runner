# ============================================
# Common settings for all plugins
# ============================================
if(WIN32)
    add_compile_definitions(PLUGIN_EXPORTS)
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=default")
endif()

# Plugin output directory
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

include_directories(include)

# ============================================
# Helper function to add a plugin target
# ============================================
function(add_plugin PLUGIN_NAME)
    add_library(${PLUGIN_NAME} SHARED ${ARGN})
    target_link_libraries(${PLUGIN_NAME}
            PUBLIC test_engine
    )
    set_target_properties(${PLUGIN_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
            RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
            PREFIX ""
    )
endfunction()


# ============================================
# Plugins
# ============================================
add_plugin(plugin_scan_class src/scan-test-correctness.cpp)