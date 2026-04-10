# ============================================================================
# Network blocking - prevents student CMake from downloading anything at
# configure time. Loaded by the engine's runner-build wrapper before
# add_subdirectory(solution).
# ============================================================================

set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

macro(ExternalProject_Add)
    message(FATAL_ERROR
        "BLOCKED: ExternalProject_Add() is not allowed. "
        "External downloads are prohibited in student solutions.")
endmacro()

macro(FetchContent_Declare)
    message(FATAL_ERROR
        "BLOCKED: FetchContent_Declare() is not allowed. "
        "External downloads are prohibited in student solutions.")
endmacro()

# NOTE: file(DOWNLOAD/UPLOAD) and execute_process are caught earlier by
# CMakeValidator's static scan of the student CMake files. We do NOT wrap
# the file() command at runtime - wrapping it breaks file(GLOB) (variable
# scope) and file(WRITE ...) of compiler-detection probes (escape mangling).
