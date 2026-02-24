#include <algorithm>
#include <cmake_generator.h>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

static std::string normalizePath(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

CMakeGenerator::CMakeGenerator(Config config)
    : config_(std::move(config)) {}

// ============================================================================
// Reusable building blocks
// ============================================================================

std::string CMakeGenerator::cmakeHeader(const std::string& project_name) {
    std::ostringstream cmake;
    cmake << "cmake_minimum_required(VERSION 3.14)\n"
          << "project(" << project_name << " CXX)\n"
          << "set(CMAKE_CXX_STANDARD 17)\n"
          << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";
    return cmake.str();
}

std::string CMakeGenerator::importedLibrary(
    const std::string& name,
    const std::string& lib_path,
    const std::string& include_path) const
{
    std::string lib = normalizePath(lib_path);
    std::string inc = normalizePath(include_path);

    std::ostringstream cmake;
    cmake << "add_library(" << name << " SHARED IMPORTED GLOBAL)\n"
          << "set_target_properties(" << name << " PROPERTIES\n"
          << "    IMPORTED_LOCATION \"" << lib << "\"\n";
#ifdef _WIN32
    {
        std::string implib = lib;
        auto dot = implib.rfind('.');
        if(dot != std::string::npos) implib = implib.substr(0, dot) + ".dll.a";
        cmake << "    IMPORTED_IMPLIB \"" << implib << "\"\n";
    }
#endif
    cmake << "    INTERFACE_INCLUDE_DIRECTORIES \"" << inc << "\"\n"
          << ")\n\n";
    return cmake.str();
}

std::string CMakeGenerator::openmpLink(const std::string& target_name) const {
    std::ostringstream cmake;
    cmake << "find_package(OpenMP REQUIRED)\n"
          << "set_property(TARGET " << target_name << " APPEND PROPERTY\n"
          << "    INTERFACE_LINK_LIBRARIES OpenMP::OpenMP_CXX)\n\n";
    return cmake.str();
}

// ============================================================================
// Composite generators
// ============================================================================

std::string CMakeGenerator::testWrapperCMakeLists(
    const std::string& test_dir,
    const std::string& student_lib_dir,
    const std::string& student_include_dir,
    bool force_auto_generate) const
{
    std::string asgn = normalizePath(test_dir);
    std::string sol_lib_dir = normalizePath(student_lib_dir);
    std::string sol_inc_dir = normalizePath(student_include_dir);

    std::ostringstream cmake;
    cmake << cmakeHeader("test_build");

    cmake << "# Import pre-built test_engine\n"
          << importedLibrary("test_engine",
                             config_.engine_lib_path,
                             config_.engine_include_path);
    cmake << "# Import pre-built parallel_lib\n"
          << importedLibrary("parallel_lib",
                             config_.parallel_lib_path,
                             config_.parallel_include_path);
    cmake << openmpLink("parallel_lib");

    // Import the real student_solution DLL
    cmake << "# Import student_solution (real DLL)\n"
          << "add_library(student_solution SHARED IMPORTED GLOBAL)\n";

#ifdef _WIN32
    cmake << "file(GLOB _sol_dll \"" << sol_lib_dir << "/libstudent_solution*.dll\")\n"
          << "file(GLOB _sol_implib \"" << sol_lib_dir << "/libstudent_solution*.dll.a\")\n"
          << "set_target_properties(student_solution PROPERTIES\n"
          << "    IMPORTED_LOCATION \"${_sol_dll}\"\n"
          << "    IMPORTED_IMPLIB \"${_sol_implib}\"\n";
#else
    cmake << "file(GLOB _sol_so \"" << sol_lib_dir << "/libstudent_solution*.so\")\n"
          << "set_target_properties(student_solution PROPERTIES\n"
          << "    IMPORTED_LOCATION \"${_sol_so}\"\n";
#endif

    cmake << "    INTERFACE_INCLUDE_DIRECTORIES \"" << sol_inc_dir << "\"\n"
          << ")\n\n";

    cmake << "link_libraries(student_solution)\n\n";

    bool use_custom = !force_auto_generate && fs::exists(fs::path(test_dir) / "CMakeLists.txt");

    if(use_custom) {
        cmake << "# Custom test build\n"
              << "add_subdirectory(\"" << asgn << "\" tests_build)\n"
              << "\n# Override output dirs — force plugins into build tree\n"
              << "get_property(_test_targets DIRECTORY \"" << asgn << "\" PROPERTY BUILDSYSTEM_TARGETS)\n"
              << "foreach(_tgt ${_test_targets})\n"
              << "    set_target_properties(${_tgt} PROPERTIES\n"
              << "        LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/plugins\"\n"
              << "        RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/plugins\"\n"
              << "    )\n"
              << "endforeach()\n";
    } else {
        cmake << "# Auto-generate test plugins from src\n"
              << "file(GLOB TEST_SOURCES \"" << asgn << "/src/*.cpp\")\n"
              << "foreach(TEST_SRC ${TEST_SOURCES})\n"
              << "    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)\n"
              << "    set(PLUGIN_NAME \"plugin_${TEST_NAME}\")\n"
              << "    add_library(${PLUGIN_NAME} SHARED ${TEST_SRC})\n"
              << "    target_link_libraries(${PLUGIN_NAME} PUBLIC\n"
              << "        test_engine student_solution parallel_lib OpenMP::OpenMP_CXX)\n";

#ifdef _WIN32
        cmake << "    target_compile_definitions(${PLUGIN_NAME} PRIVATE PLUGIN_EXPORTS)\n"
              << "    set_target_properties(${PLUGIN_NAME} PROPERTIES "
              << "WINDOWS_EXPORT_ALL_SYMBOLS ON)\n";
#else
        cmake << "    target_compile_options(${PLUGIN_NAME} PRIVATE "
              << "-fPIC -fvisibility=default)\n";
#endif

        cmake << "    set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX \"\")\n"
              << "endforeach()\n";
    }

    return cmake.str();
}

std::string CMakeGenerator::solutionWrapperCMakeLists() const
{
    std::ostringstream cmake;
    cmake << cmakeHeader("solution_build");

    cmake << "include(${CMAKE_CURRENT_SOURCE_DIR}/block_network.cmake)\n\n";

    cmake << "# Import parallel_lib\n"
          << importedLibrary("parallel_lib",
                             config_.parallel_lib_path,
                             config_.parallel_include_path);
    cmake << openmpLink("parallel_lib");

    cmake << "add_subdirectory(solution)\n\n"
          << "if(TARGET student_solution)\n"
          << "    set_target_properties(student_solution PROPERTIES POSITION_INDEPENDENT_CODE ON)\n"
          << "    if(WIN32)\n"
          << "        set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)\n"
          << "    endif()\n"
          << "else()\n"
          << "    message(FATAL_ERROR \"Student CMakeLists.txt must define 'student_solution' target\")\n"
          << "endif()\n";

    return cmake.str();
}

std::string CMakeGenerator::defaultSolutionCMakeLists() {
    return
        "file(GLOB_RECURSE SOLUTION_SOURCES\n"
        "    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp\n"
        "    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)\n"
        "add_library(student_solution SHARED ${SOLUTION_SOURCES})\n"
        "target_include_directories(student_solution PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)\n"
        "target_link_libraries(student_solution PUBLIC parallel_lib)\n"
        "if(WIN32)\n"
        "    set_target_properties(student_solution PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)\n"
        "endif()\n";
}

std::string CMakeGenerator::networkBlockScript() {
    return R"cmake(
# ============================================
# Network blocking — prevents student code from downloading anything
# ============================================

# Block FetchContent
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# Block ExternalProject_Add if anyone tries to use it
macro(ExternalProject_Add)
    message(FATAL_ERROR
        "BLOCKED: ExternalProject_Add() is not allowed. "
        "External downloads are prohibited in student solutions.")
endmacro()

# Block FetchContent_Declare (even though FULLY_DISCONNECTED is set,
# this provides a clear error message)
macro(FetchContent_Declare)
    message(FATAL_ERROR
        "BLOCKED: FetchContent_Declare() is not allowed. "
        "External downloads are prohibited in student solutions.")
endmacro()

# Block find_package for package managers that could trigger downloads
# (conan, vcpkg, etc). Standard system packages are allowed.
)cmake";
}
