# Package config for ParlayLib, shipped alongside the engine.
#
# Location at runtime: <engine>/cmake/parlay/parlayConfig.cmake
# Headers at runtime:  <engine>/include/parlay/...
#
# Mirrors parlaylib's upstream target name `parlay` so student CMake
# can use the textbook find_package form without any engine-specific
# vocabulary:
#
#     find_package(parlay CONFIG REQUIRED)
#     target_link_libraries(my_target PUBLIC parlay)

if(TARGET parlay)
    return()
endif()

get_filename_component(_parlay_include
    "${CMAKE_CURRENT_LIST_DIR}/../../include" ABSOLUTE)

add_library(parlay INTERFACE IMPORTED)
set_target_properties(parlay PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_parlay_include}")

unset(_parlay_include)
