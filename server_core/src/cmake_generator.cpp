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
        if(!f)
            throw std::runtime_error(
                "CMakeGenerator: failed to open template " + path.string()
                + " (did the engine ship cmake/ next to the executable?)"
            );
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    /// Simple @KEY@ substitution. Every key in `vars` is replaced everywhere in
    /// `text`; placeholders left unsubstituted are an error (template / code drift).
    std::string substitute(
        std::string text,
        const std::unordered_map<std::string, std::string>& vars
    ) {
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
                        "CMakeGenerator: unsubstituted placeholder " + marker + " in template"
                    );
                }
            }
        }
        return text;
    }

    /// Generate `set_target_properties(name PROPERTIES IMPORTED_LOCATION ... IMPORTED_IMPLIB ... INTERFACE_INCLUDE_DIRECTORIES ...)`
    /// block for a STATIC IMPORTED archive (used by framework runner_* libs).
    std::string static_imported(
        const std::string& name,
        const std::string& lib_path,
        const std::string& include_path
    ) {
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
    std::string shared_imported(
        const std::string& name,
        const std::string& lib_path,
        const std::string& include_path
    ) {
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
    config_.engine_lib_path = make_absolute(config_.engine_lib_path);
    config_.engine_include_path = make_absolute(config_.engine_include_path);
    config_.parallel_lib_path = make_absolute(config_.parallel_lib_path);
    config_.parallel_include_path = make_absolute(config_.parallel_include_path);
    config_.runner_lib_path = make_absolute(config_.runner_lib_path);
    config_.runner_include_path = make_absolute(config_.runner_include_path);
    config_.shadow_omp_dir = make_absolute(config_.shadow_omp_dir);
    config_.runner_omp_source_path = make_absolute(config_.runner_omp_source_path);
    config_.runner_parlay_source_path = make_absolute(config_.runner_parlay_source_path);
    config_.runner_cilk_source_path = make_absolute(config_.runner_cilk_source_path);
    config_.runner_seq_source_path = make_absolute(config_.runner_seq_source_path);
    config_.parlay_headers_path = make_absolute(config_.parlay_headers_path);
    config_.template_dir = make_absolute(config_.template_dir);
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
    //
    // Each framework's runner_<variant>.cpp is compiled here (at student-build
    // time) by the *same* compiler that builds the student's solution. This
    // is mandatory for Cilk (cilk_for is an OpenCilk-clang-only keyword) and
    // gives architectural symmetry for the others: each variant uses its
    // framework's native parallelism dialect for setup-time warmup.
    auto compile_variant_from_source = [&](
        const std::string& target,
        const std::string& source_path
    ) {
        std::ostringstream out;
        out << "add_library(" << target << " STATIC \""
            << normalize_path(source_path) << "\")\n"
            << "target_link_libraries(" << target << " PUBLIC runner_lib)\n"
            << "target_include_directories(" << target
            << " PRIVATE \"" << normalize_path(runner_inc) << "\")\n";
        return out.str();
    };

    std::string framework_block;
    std::string runner_variant;
    std::string student_extra_link;
    // shadow_omp injection - target-scoped (only student + teacher's runner exe).
    // Earlier we put this on global `include_directories(BEFORE SYSTEM ...)`,
    // which also hit our own runner_omp.cpp (engine code) and forced an
    // `extern "C"` workaround. Scoping per-target keeps the assignment guard
    // exactly where it belongs - student-written translation units.
    std::string shadow_omp_for_student;
    std::string shadow_omp_for_runner;
    if(framework == "openmp" && shadow_omp) {
        const std::string shadow = normalize_path(config_.shadow_omp_dir);
        shadow_omp_for_student =
            "target_include_directories(student_solution BEFORE PRIVATE \"" + shadow + "\")\n";
        shadow_omp_for_runner =
            "target_include_directories(runner BEFORE PRIVATE \"" + shadow + "\")\n";
    }

    if(framework == "openmp") {
        runner_variant = "runner_omp";
        framework_block = shared_imported(
            "parallel_lib",
            config_.parallel_lib_path,
            config_.parallel_include_path
        );
        framework_block += "\nfind_package(OpenMP REQUIRED)\n";
        framework_block += compile_variant_from_source(
            "runner_omp",
            config_.runner_omp_source_path
        );
        framework_block += "target_link_libraries(runner_omp PUBLIC parallel_lib OpenMP::OpenMP_CXX)\n";
        student_extra_link =
            "target_link_libraries(student_solution PUBLIC parallel_lib OpenMP::OpenMP_CXX)\n"
            + shadow_omp_for_student;
    } else if(framework == "parlay") {
        runner_variant = "runner_parlay";
        framework_block = compile_variant_from_source(
            "runner_parlay",
            config_.runner_parlay_source_path
        );
        if(!config_.parlay_headers_path.empty()) {
            // Point parlay_DIR at our shipped parlayConfig.cmake so both this
            // wrapper and the student's CMake can pull parlay in via plain
            // `find_package(parlay CONFIG REQUIRED)`. No engine-specific
            // vocabulary leaks into the student's CMakeLists.
            const fs::path parlay_cmake_dir =
                fs::path(config_.parlay_headers_path).parent_path() / "cmake" / "parlay";
            framework_block += "set(parlay_DIR \""
                + normalize_path(parlay_cmake_dir.string()) + "\")\n";
            framework_block += "find_package(parlay CONFIG REQUIRED)\n";
            framework_block += "target_link_libraries(runner_parlay PRIVATE parlay)\n";
        }
    } else if(framework == "cilk") {
        runner_variant = "runner_cilk";
        framework_block = compile_variant_from_source(
            "runner_cilk",
            config_.runner_cilk_source_path
        );
        // cilk_for keyword needs -fopencilk explicitly. (CMAKE_CXX_COMPILER is
        // already the OpenCilk clang set by BuildService when framework=cilk.)
        framework_block += "target_compile_options(runner_cilk PRIVATE -fopencilk)\n";
    } else if(framework == "none") {
        runner_variant = "runner_seq";
        framework_block = compile_variant_from_source(
            "runner_seq",
            config_.runner_seq_source_path
        );
    } else {
        throw std::runtime_error("CMakeGenerator: unknown framework '" + framework + "'");
    }

    // ---- @EXTRA_LIB_DIRS_BLOCK@ ----
    // Runs *after* the `runner` target is defined in the template, so this is
    // also where we attach target-scoped shadow_omp for the runner exe (which
    // pulls in teacher's main.cpp - it must obey assignment-level OpenMP
    // restrictions just like student_solution does).
    std::ostringstream extra_out;
    extra_out << shadow_omp_for_runner;
    if(!extra_lib_dirs.empty()) {
        extra_out << "# Teacher-bundled headers (tests/libs/<*>/)\n";
        for(const auto& dir : extra_lib_dirs) {
            extra_out << "target_include_directories(runner PRIVATE \""
                << normalize_path(dir) << "\")\n";
        }
    }
    std::string extra_lib_dirs_block = extra_out.str();

    std::unordered_map<std::string, std::string> vars{
        {"CILK_FLAGS_BLOCK", cilk_flags_block},
        {"RUNNER_LIB_PATH", normalize_path(config_.runner_lib_path)},
        {"RUNNER_LIB_INCLUDE", runner_inc},
        {"FRAMEWORK_BLOCK", framework_block},
        {"STUDENT_EXTRA_LINK", student_extra_link},
        {"RUNNER_VARIANT", runner_variant},
        {"RUNNER_MAIN_PATH", normalize_path(runner_main_path)},
        {"TEST_INCLUDE_DIR", normalize_path(test_include_dir)},
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
    platform_global = "set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON CACHE BOOL \"\" FORCE)";
    #else
    iface_flags_line = "INTERFACE_COMPILE_OPTIONS \"-fvisibility=default\"";
    platform_global = "set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL \"\" FORCE)";
    #endif

    // INTERFACE_INCLUDE_DIRECTORIES on test_engine carries both its own headers
    // AND runner_lib's <test_data.h> - Test/TestData live across both libs.
    std::string includes = normalize_path(config_.engine_include_path) + ";" + runner_inc;

    std::unordered_map<std::string, std::string> vars{
        {"TEST_ENGINE_LIB", normalize_path(config_.engine_lib_path)},
        {"TEST_ENGINE_IMPLIB_LINE", implib_line},
        {"TEST_ENGINE_INTERFACE_FLAGS", iface_flags_line},
        {"TEST_ENGINE_INCLUDES", includes},
        {"RUNNER_LIB_PATH", normalize_path(config_.runner_lib_path)},
        {"RUNNER_LIB_INCLUDE", runner_inc},
        {"PLATFORM_GLOBAL", platform_global},
        {"TEACHER_TESTS_DIR", normalize_path(test_dir)},
    };

    fs::path tpl = fs::path(config_.template_dir) / "test_plugin_wrapper.cmake.in";
    return substitute(read_file(tpl), vars);
}
