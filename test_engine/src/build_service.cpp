#include <algorithm>
#include <atomic>
#include <build_service.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <process_utils.h>
#include <sstream>

namespace fs = std::filesystem;


namespace {

    #ifdef _WIN32
    constexpr const char* SHARED_LIB_EXT = ".dll";
    #else
    constexpr const char* SHARED_LIB_EXT = ".so";
    #endif

    std::atomic<int> temp_counter{0};

    /// Reliable recursive directory copy (fs::copy with recursive is unreliable on MinGW).
    void copyDirectoryRecursive(const fs::path& src, const fs::path& dst, std::error_code& ec) {
        fs::create_directories(dst, ec);
        if(ec) return;

        for(const auto& entry : fs::directory_iterator(src, ec)) {
            if(ec) return;
            // Skip symlinks to prevent symlink attacks from cloned repos
            if(entry.is_symlink()) continue;
            const auto& target = dst / entry.path().filename();
            if(entry.is_directory()) {
                copyDirectoryRecursive(entry.path(), target, ec);
                if(ec) return;
            } else {
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
                if(ec) return;
            }
        }
    }

    /// Run cmake configure + build, return combined output. Sets error on failure.
    std::string cmakeConfigureAndBuild(
        const std::string& cmake_executable,
        const std::string& generator,
        const fs::path& source_dir,
        const fs::path& build_dir,
        std::string& error_message,
        const std::string& extra_defines = ""
    ) {
        std::string output;

        std::string configure_cmd = "\"" + cmake_executable + "\" "
            + "-G \"" + generator + "\" "
            + "-DCMAKE_BUILD_TYPE=Release "
            + extra_defines
            + "\"" + source_dir.string() + "\" "
            + "-B \"" + build_dir.string() + "\" "
            + " 2>&1";

        auto configure_result = runCommand(configure_cmd);
        output += "=== Configure ===\n" + configure_result.output + "\n";

        if(configure_result.failed()) {
            error_message = "CMake configure failed";
            return output;
        }

        std::string build_cmd = "\"" + cmake_executable + "\" "
            + "--build \"" + build_dir.string() + "\" "
            + "--config Release "
            + " 2>&1";

        auto build_result = runCommand(build_cmd);
        output += "=== Build ===\n" + build_result.output + "\n";

        if(build_result.failed()) {
            error_message = "Build failed";
            return output;
        }

        return output;
    }

} // anonymous namespace

BuildService::BuildService(BuildConfig config)
    : config_(config),
      cmake_gen_(
          {
              config.engine_lib_path,
              config.engine_include_path,
              config.parallel_lib_path,
              config.parallel_include_path,
              config.memory_inject_path
          }
      ) {}

void BuildService::cleanup(const std::string& build_dir) {
    if(build_dir.empty()) return;
    std::error_code ec;
    fs::remove_all(build_dir, ec);
    if(ec) {
        std::cerr << "[BuildService] Cleanup warning: " << ec.message() << "\n";
    }
}

// ============================================================================
// buildTests — build test plugins against the real student DLL
// ============================================================================

BuildService::TestBuildResult BuildService::buildTests(
    const std::string& test_dir,
    const std::string& student_lib_dir,
    const std::string& student_include_dir
) {
    TestBuildResult result;

    if(!fs::exists(test_dir)) {
        result.error_message = "Test directory not found: " + test_dir;
        return result;
    }

    // Create temp directory for the test build
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    fs::path tmp_tests = fs::temp_directory_path() / ("tests-" + std::to_string(now) + "-" + std::to_string(
        temp_counter.fetch_add(1)
    ));
    {
        std::error_code ec;
        fs::remove_all(tmp_tests, ec);
    }
    fs::create_directories(tmp_tests);
    result.build_dir = tmp_tests.string();

    std::cout << "[BuildService] Building test plugins against student DLL...\n";

    {
        std::string cmake_content = cmake_gen_.testWrapperCMakeLists(
            test_dir,
            student_lib_dir,
            student_include_dir
        );
        std::ofstream f(tmp_tests / "CMakeLists.txt");
        f << cmake_content;
    }

    // Build tests — internet ALLOWED (no network blocking)
    fs::path test_build_dir = tmp_tests / "build";
    result.build_output += cmakeConfigureAndBuild(
        config_.cmake_executable,
        config_.generator,
        tmp_tests,
        test_build_dir,
        result.error_message
    );

    // Fallback: if custom CMakeLists failed, retry with auto-generated
    if(!result.error_message.empty() && fs::exists(fs::path(test_dir) / "CMakeLists.txt")) {
        std::string first_error = result.error_message;
        std::string first_output = result.build_output;
        result.error_message.clear();

        std::cerr << "[BuildService] Test CMakeLists failed, trying auto-generated fallback...\n";

        {
            std::error_code ec;
            fs::remove_all(test_build_dir, ec);
        }

        {
            std::string cmake_content = cmake_gen_.testWrapperCMakeLists(
                test_dir,
                student_lib_dir,
                student_include_dir,
                true
            );
            std::ofstream f(tmp_tests / "CMakeLists.txt");
            f << cmake_content;
        }

        result.build_output = first_output
            + "\n[FALLBACK] " + first_error
            + "\n\n=== Retry with auto-generated test CMakeLists ===\n";

        result.build_output += cmakeConfigureAndBuild(
            config_.cmake_executable,
            config_.generator,
            tmp_tests,
            test_build_dir,
            result.error_message
        );

        if(result.error_message.empty()) {
            result.cmake_replaced = true;
            result.cmake_replaced_reason = first_error + "; replaced with auto-generated";
        }
    }

    if(!result.error_message.empty()) {
        result.build_output += "[BuildService] Test plugins build failed\n";
        return result;
    }

    // Collect plugin DLLs from the build
    for(const auto& entry : fs::recursive_directory_iterator(test_build_dir)) {
        if(!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if(entry.path().extension().string() != SHARED_LIB_EXT) continue;

        if(filename.find("plugin_") != std::string::npos) {
            result.plugin_paths.push_back(entry.path().string());
        }
    }

    if(result.plugin_paths.empty()) {
        result.error_message = "No plugin libraries found after test build";
        return result;
    }

    result.success = true;
    std::cout << "[BuildService] Test plugins built. Plugins: "
        << result.plugin_paths.size() << "\n";
    return result;
}

// ============================================================================
// buildSolution — build only student solution (network blocked)
// ============================================================================

BuildService::SolutionBuildResult BuildService::buildSolution(
    const std::string& solution_dir,
    const std::string& test_dir,
    const std::string& job_id
) {
    SolutionBuildResult result;

    fs::path tmp = fs::temp_directory_path() / ("build-sol-" + job_id);
    {
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }
    fs::create_directories(tmp);
    result.build_dir = tmp.string();

    std::cout << "[BuildService] Building solution for job " << job_id << "\n";

    // Copy student solution
    fs::path solution_workspace = tmp / "solution";
    {
        std::error_code ec;
        copyDirectoryRecursive(fs::path(solution_dir), solution_workspace, ec);
        if(ec) {
            result.error_message = "Failed to copy solution to workspace: " + ec.message();
            return result;
        }
    }

    // Student's own headers — no overwriting
    result.solution_include_dir = (solution_workspace / "include").string();

    {
        std::ofstream f(tmp / "block_network.cmake");
        f << CMakeGenerator::networkBlockScript();
    }

    // Write wrapper CMakeLists.txt
    auto write_wrapper = [&]() {
        std::ofstream f(tmp / "CMakeLists.txt");
        f << cmake_gen_.solutionWrapperCMakeLists();
    };

    write_wrapper();

    // --- First attempt: use student's own CMakeLists.txt ---
    fs::path build_dir = tmp / "build";
    bool need_fallback = false;

    if(fs::exists(solution_workspace / "CMakeLists.txt")) {
        result.build_output = cmakeConfigureAndBuild(
            config_.cmake_executable,
            config_.generator,
            tmp,
            build_dir,
            result.error_message,
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON "
            "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON "
        );
        if(!result.error_message.empty()) {
            need_fallback = true;
        }
    } else {
        need_fallback = true;
    }

    // --- Fallback: replace student's CMakeLists with a default one ---
    if(need_fallback) {
        std::string first_output = result.build_output;
        std::string first_error = result.error_message;
        result.error_message.clear();

        result.cmake_replaced = true;

        std::string reason;
        if(!fs::exists(solution_workspace / "CMakeLists.txt")) {
            reason = "No CMakeLists.txt in solution";
        } else {
            reason = first_error;
        }

        std::cout << "[BuildService] Student CMakeLists failed, trying fallback...\n";

        // Priority 1: test_dir/solution-defaults/CMakeLists.txt
        fs::path custom_fallback = fs::path(test_dir) / "solution-defaults" / "CMakeLists.txt";
        if(fs::exists(custom_fallback)) {
            std::cout << "[BuildService] Using fallback from: " << custom_fallback << "\n";
            fs::copy_file(
                custom_fallback,
                solution_workspace / "CMakeLists.txt",
                fs::copy_options::overwrite_existing
            );
            reason += "; replaced with test-provided defaults";
        } else {
            // Priority 2: auto-generated default
            std::cout << "[BuildService] Using auto-generated default CMakeLists\n";
            std::ofstream f(solution_workspace / "CMakeLists.txt");
            f << CMakeGenerator::defaultSolutionCMakeLists();
            reason += "; replaced with auto-generated defaults";
        }
        result.cmake_replaced_reason = reason;

        // Clean previous build dir and retry
        {
            std::error_code ec;
            fs::remove_all(build_dir, ec);
        }

        write_wrapper();

        result.build_output = cmakeConfigureAndBuild(
            config_.cmake_executable,
            config_.generator,
            tmp,
            build_dir,
            result.error_message,
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON "
            "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON "
        );

        // Prepend first attempt output for diagnostics
        result.build_output = "=== First attempt (student CMakeLists) ===\n"
            + first_output + "\n[FALLBACK] " + first_error
            + "\n\n=== Retry with fallback CMakeLists ===\n"
            + result.build_output;

        if(!result.error_message.empty()) return result;
    }

    // Find built solution library
    for(const auto& entry : fs::recursive_directory_iterator(build_dir)) {
        if(!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if(filename.find("student_solution") != std::string::npos &&
            entry.path().extension().string() == SHARED_LIB_EXT) {
            result.solution_lib_path = entry.path().string();
            break;
        }
    }

    if(result.solution_lib_path.empty()) {
        result.error_message = "No student_solution library found after build";
        return result;
    }

    result.success = true;
    std::cout << "[BuildService] Solution build successful for job " << job_id << "\n";
    return result;
}