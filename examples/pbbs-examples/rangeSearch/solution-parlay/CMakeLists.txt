cmake_minimum_required(VERSION 3.14)
project(pbbs_rs_parlay)
set(CMAKE_CXX_STANDARD 17)
find_package(parlay CONFIG REQUIRED)
file(GLOB_RECURSE SOLUTION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(student_solution SHARED ${SOLUTION_SOURCES})
target_include_directories(student_solution PUBLIC include)
target_link_libraries(student_solution PUBLIC parlay)
# pbbs's NSGDist.h has an AVX path and a fallback SSE2 path; the SSE2
# fallback in upstream uses non-existent intrinsic names (`_mm128_*`
# rather than `_mm_*`) and won't compile. Force the AVX branch.
target_compile_options(student_solution PRIVATE -mavx2)

# ============================================================
# Platform-specific workarounds (Windows / MinGW only).
# These are no-ops on Linux and in our Docker production build.
# Safe to delete for a Linux-only deployment.
# ============================================================
if(WIN32)
    # Export all symbols from the shared lib so the engine's plugin
    # loader can find student:: entry points without DLL_EXPORT macros.
    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
if(MINGW)
    # Heavy parlay template instantiation overflows MinGW's COFF section
    # limit at -O0 (linker reports "string table overflow"). -O1 keeps
    # debug info smaller and -Wa,-mbig-obj relaxes the section cap.
    target_compile_options(student_solution PRIVATE -O1 -Wa,-mbig-obj)
endif()
