#include <algorithm>
#include <cmake_generator.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

std::string normalize_path(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

std::string make_absolute(const std::string& p) {
    if(p.empty()) return p;
    return fs::absolute(fs::path(p)).string();
}

std::string read_file(const fs::path& path) {
    std::ifstream f(path);
    if(!f) throw std::runtime_error(
        "CMakeGenerator: failed to open template " + path.string()
        + " (did the engine ship cmake/ next to the executable?)");
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/// Simple @KEY@ substitution. Every key in `vars` is replaced everywhere in
/// `text`; placeholders left unsubstituted are an error (template / code drift).
std::string substitute(std::string text,
                       const std::unordered_map<std::string, std::string>& vars) {
    for(const auto& [key, value] : vars) {
        const std::string marker = "@" + key + "@";
        std::size_t pos = 0;
        while((pos = text.find(marker, pos)) != std::string::npos) {
            text.replace(pos, marker.size(), value);
            pos += value.size();
        }
    }
    // Defensive: if the template still has @SOMETHING@ left, surface it
    // immediately rather than producing a half-rendered file that CMake
    // would reject with a confusing error.
    auto leftover = text.find('@');
    if(leftover != std::string::npos) {
        auto end = text.find('@', leftover + 1);
        if(end != std::string::npos) {
            std::string marker = text.substr(leftover, end - leftover + 1);
            // Allow literal @ in URLs / paths by requiring uppercase ASCII + underscore between @s.
            bool looks_like_placeholder = end - leftover >= 3;
            for(std::size_t i = leftover + 1; i < end && looks_like_placeholder; ++i) {
                char c = text[i];
                looks_like_placeholder = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
            }
            if(looks_like_placeholder) {
                throw std::runtime_error(
                    "CMakeGenerator: unsubstituted placeholder " + marker + " in template");
            }
        }
    }
    return text;
}

/// Generate `set_target_properties(name PROPERTIES IMPORTED_LOCATION ... IMPORTED_IMPLIB ... INTERFACE_INCLUDE_DIRECTORIES ...)`
/// block for a STATIC IMPORTED archive (used by framework runner_* libs).
std::string static_imported(const std::string& name,
                            const std::string& lib_path,
                            const std::string& include_path) {
    std::string lib = normalize_path(lib_path);
    std::string inc = normalize_path(include_path);
    std::ostringstream out;
    out << "add_library(" << name << " STATIC IMPORTED GLOBAL)\n"
        << "set_target_properties(" << name << " PROPERTIES\n"
        << "    IMPORTED_LOCATION \"" << lib << "\"\n"
        << "    IMPORTED_LOCATION_RELEASE \"" << lib << "\"\n"
        << "    IMPORTED_LOCATION_DEBUG \"" << lib << "\"\n"
        << "    INTERFACE_INCLUDE_DIRECTORIES \"" << inc << "\"\n"
        << ")\n";
    return out.str();
}

/// Generate the same block for a SHARED IMPORTED .dll/.so (the OpenMP variant
/// links parallel_lib this way; on Windows the import library (.dll.a) sits
/// next to the .dll and must be wired up explicitly).
std::string shared_imported(const std::string& name,
                            const std::string& lib_path,
                            const std::string& include_path) {
    std::string lib = normalize_path(lib_path);
    std::string inc = normalize_path(include_path);
    std::ostringstream out;
    out << "add_library(" << name << " SHARED IMPORTED GLOBAL)\n"
        << "set_target_properties(" << name << " PROPERTIES\n"
        << "    IMPORTED_LOCATION \"" << lib << "\"\n";
    #ifdef _WIN32
    {
        fs::path lp(lib);
        std::string implib = (lp.parent_path() / lp.stem()).string() + ".dll.a";
        out << "    IMPORTED_IMPLIB \"" << normalize_path(implib) << "\"\n";
    }
    #endif
    out << "    IMPORTED_LOCATION_RELEASE \"" << lib << "\"\n"
        << "    IMPORTED_LOCATION_DEBUG \"" << lib << "\"\n"
        << "    INTERFACE_INCLUDE_DIRECTORIES \"" << inc << "\"\n"
        << ")\n";
    return out.str();
}

} // namespace

// ============================================================================
// Construction
// ============================================================================

CMakeGenerator::CMakeGenerator(Config config)
    : config_(std::move(config)) {
    // All paths used in generated CMake must be absolute - the wrapper runs
    // from a fresh temp dir without context.
    config_.engine_lib_path        = make_absolute(config_.engine_lib_path);
    config_.engine_include_path    = make_absolute(config_.engine_include_path);
    config_.parallel_lib_path      = make_absolute(config_.parallel_lib_path);
    config_.parallel_include_path  = make_absolute(config_.parallel_include_path);
    config_.runner_lib_path        = make_absolute(config_.runner_lib_path);
    config_.runner_include_path    = make_absolute(config_.runner_include_path);
    config_.shadow_omp_dir         = make_absolute(config_.shadow_omp_dir);
    config_.runner_omp_lib_path    = make_absolute(config_.runner_omp_lib_path);
    config_.runner_parlay_lib_path = make_absolute(config_.runner_parlay_lib_path);
    config_.runner_cilk_lib_path   = make_absolute(config_.runner_cilk_lib_path);
    config_.runner_seq_lib_path    = make_absolute(config_.runner_seq_lib_path);
    config_.parlay_headers_path    = make_absolute(config_.parlay_headers_path);
    config_.template_dir           = make_absolute(config_.template_dir);
}

// ============================================================================
// runner_cmake_lists - substitute runner_wrapper.cmake.in
// ============================================================================

std::string CMakeGenerator::runner_cmake_lists(
    const std::string& framework,
    const std::string& runner_main_path,
    const std::string& test_include_dir,
    const std::vector<std::string>& extra_lib_dirs,
    bool shadow_omp
) const {
    const std::string runner_inc = normalize_path(config_.runner_include_path);

    // ---- @CILK_FLAGS_BLOCK@ ----
    std::string cilk_flags_block;
    if(framework == "cilk") {
        cilk_flags_block = "add_compile_options(-fopencilk)\n"
                           "add_link_options(-fopencilk)\n";
    }

    // ---- @FRAMEWORK_BLOCK@ + @RUNNER_VARIANT@ + @STUDENT_EXTRA_LINK@ ----
    std::string framework_block;
    std::string runner_variant;
    std::string student_extra_link;
    if(framework == "openmp") {
        runner_variant = "runner_omp";
        framework_block  = static_imported("runner_omp", config_.runner_omp_lib_path, runner_inc);
        framework_block += "\n";
        framework_block += shared_imported("parallel_lib",
                                           config_.parallel_lib_path,
                                           config_.parallel_include_path);
        framework_block += "\nfind_package(OpenMP REQUIRED)\n";
        if(shadow_omp) {
            framework_block += "include_directories(BEFORE SYSTEM \""
                            + normalize_path(config_.shadow_omp_dir) + "\")\n";
        }
        student_extra_link =
            "target_link_libraries(student_solution PUBLIC parallel_lib OpenMP::OpenMP_CXX)";
    } else if(framework == "parlay") {
        runner_variant = "runner_parlay";
        framework_block = static_imported("runner_parlay",
                                          config_.runner_parlay_lib_path,
                                          runner_inc);
        if(!config_.parlay_headers_path.empty()) {
            framework_block += "include_directories(\""
                            + normalize_path(config_.parlay_headers_path) + "\")\n";
        }
    } else if(framework == "cilk") {
        runner_variant  = "runner_cilk";
        framework_block = static_imported("runner_cilk", config_.runner_cilk_lib_path, runner_inc);
    } else if(framework == "none") {
        runner_variant  = "runner_seq";
        framework_block = static_imported("runner_seq", config_.runner_seq_lib_path, runner_inc);
    } else {
        throw std::runtime_error("CMakeGenerator: unknown framework '" + framework + "'");
    }

    // ---- @EXTRA_LIB_DIRS_BLOCK@ ----
    std::string extra_lib_dirs_block;
    if(!extra_lib_dirs.empty()) {
        std::ostringstream out;
        out << "# Teacher-bundled headers (tests/libs/<*>/)\n";
        for(const auto& dir : extra_lib_dirs) {
            out << "target_include_directories(runner PRIVATE \""
                << normalize_path(dir) << "\")\n";
        }
        extra_lib_dirs_block = out.str();
    }

    std::unordered_map<std::string, std::string> vars{
        {"CILK_FLAGS_BLOCK",     cilk_flags_block},
        {"RUNNER_LIB_PATH",      normalize_path(config_.runner_lib_path)},
        {"RUNNER_LIB_INCLUDE",   runner_inc},
        {"FRAMEWORK_BLOCK",      framework_block},
        {"STUDENT_EXTRA_LINK",   student_extra_link},
        {"RUNNER_VARIANT",       runner_variant},
        {"RUNNER_MAIN_PATH",     normalize_path(runner_main_path)},
        {"TEST_INCLUDE_DIR",     normalize_path(test_include_dir)},
        {"EXTRA_LIB_DIRS_BLOCK", extra_lib_dirs_block},
    };

    fs::path tpl = fs::path(config_.template_dir) / "runner_wrapper.cmake.in";
    return substitute(read_file(tpl), vars);
}

// ============================================================================
// test_plugin_cmake_lists - substitute test_plugin_wrapper.cmake.in
// ============================================================================

std::string CMakeGenerator::test_plugin_cmake_lists(
    const std::string& test_dir,
    const std::string& test_include_dir
) const {
    (void)test_include_dir;   // teacher's CMakeLists owns its include paths

    const std::string runner_inc = normalize_path(config_.runner_include_path);

    std::string implib_line;
    std::string iface_flags_line;
    std::string platform_global;

    #ifdef _WIN32
    {
        fs::path lp(config_.engine_lib_path);
        std::string implib = (lp.parent_path() / lp.stem()).string() + ".dll.a";
        implib_line = "IMPORTED_IMPLIB \"" + normalize_path(implib) + "\"";
    }
    iface_flags_line = "INTERFACE_COMPILE_DEFINITIONS \"PLUGIN_EXPORTS\"";
    platform_global  = "set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON CACHE BOOL \"\" FORCE)";
    #else
    iface_flags_line = "INTERFACE_COMPILE_OPTIONS \"-fvisibility=default\"";
    platform_global  = "set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL \"\" FORCE)";
    #endif

    // INTERFACE_INCLUDE_DIRECTORIES on test_engine carries both its own headers
    // AND runner_lib's <test_data.h> - Test/TestData live across both libs.
    std::string includes = normalize_path(config_.engine_include_path) + ";" + runner_inc;

    std::unordered_map<std::string, std::string> vars{
        {"TEST_ENGINE_LIB",             normalize_path(config_.engine_lib_path)},
        {"TEST_ENGINE_IMPLIB_LINE",     implib_line},
        {"TEST_ENGINE_INTERFACE_FLAGS", iface_flags_line},
        {"TEST_ENGINE_INCLUDES",        includes},
        {"RUNNER_LIB_PATH",             normalize_path(config_.runner_lib_path)},
        {"RUNNER_LIB_INCLUDE",          runner_inc},
        {"PLATFORM_GLOBAL",             platform_global},
        {"TEACHER_TESTS_DIR",           normalize_path(test_dir)},
    };

    fs::path tpl = fs::path(config_.template_dir) / "test_plugin_wrapper.cmake.in";
    return substitute(read_file(tpl), vars);
}
