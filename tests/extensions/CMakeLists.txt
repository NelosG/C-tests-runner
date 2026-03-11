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
set(PLUGIN_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output-${CMAKE_BUILD_TYPE}/plugins)
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

# ============================================
# Helper function to add a plugin target
# ============================================
function(add_plugin PLUGIN_NAME)
    add_library(${PLUGIN_NAME} SHARED ${ARGN})
    target_link_libraries(${PLUGIN_NAME}
            PUBLIC test_engine
            PUBLIC student_solution
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
add_plugin(plugin_correctness src/test_correctness.cpp)
add_plugin(plugin_edge_cases src/test_edge_cases.cpp)
add_plugin(plugin_stability_scalability src/test_stability.cpp src/test_scalability.cpp)
add_plugin(plugin_parallel_verify src/test_parallel_verify.cpp)
target_link_libraries(plugin_parallel_verify PUBLIC parallel_lib)
