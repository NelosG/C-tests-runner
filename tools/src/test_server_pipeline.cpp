/**
 * @file test_server_pipeline.cpp
 * @brief Integration test: exercises the full server pipeline without HTTP.
 *
 * Usage:
 *   ./test_server_pipeline --test-id <id> [--test-dir <dir>]
 *                          --solution <dir> [--solution <dir2> ...]
 *                          [--engine-lib <path>] [--engine-include <path>]
 *                          [--parallel-lib <path>] [--parallel-include <path>]
 *                          [--mode correctness|performance|all]
 *                          [--threads N] [--no-cleanup]
 */

#include <build_service.h>
#include <filesystem>
#include <iostream>
#include <plugin_loader.h>
#include <project_utils.h>
#include <string>
#include <test_engine.h>
#include <test_registry.h>
#include <test_scenario_result_converter.h>
#include <thread_counts.h>
#include <vector>
#include <nlohmann/json.hpp>
#include <par/monitor.h>

namespace fs = std::filesystem;

static void printUsage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " [options]\n"
        << "Options:\n"
        << "  --test-id <id>             Test identifier (required)\n"
        << "  --test-dir <dir>           Path to test directory\n"
        << "  --solution <dir>           Path to student solution (repeatable)\n"
        << "  --engine-lib <path>        Path to libtest_engine.so / test_engine.dll\n"
        << "  --engine-include <path>    Path to test_engine header directory\n"
        << "  --parallel-lib <path>      Path to libparallel_lib.so / parallel_lib.dll\n"
        << "  --parallel-include <path>  Path to parallel_lib include directory\n"
        << "  --mode <mode>              correctness | performance | all (default: all)\n"
        << "  --threads <N>              Number of OMP threads (default: 4)\n"
        << "  --no-cleanup               Don't remove temp build directory\n";
}

static std::string findLib(const fs::path& project_root, const std::string& name) {
    std::vector<fs::path> candidates = {
        project_root / "output-Debug" / name,
        project_root / "output-Release" / name,
        project_root / "output-" / name,
        project_root / "cmake-build-debug" / name,
        project_root / "cmake-build-release" / name,
        project_root / "build" / name,
    };
    for(const auto& p : candidates) {
        if(fs::exists(p)) return p.string();
    }
    return "";
}

/// Run one solution through the full pipeline. Returns {total, passed}.
static std::pair<int, int> runSolution(
    const std::string& solution_dir,
    const std::string& test_dir,
    const std::string& mode,
    int threads,
    bool do_cleanup,
    BuildService& builder
) {
    std::string sol_label = fs::path(solution_dir).filename().string();
    std::cout << "\n========== Solution: " << sol_label << " ==========\n";

    // Step 1: Build solution
    std::string job_id = "test-" + sol_label;
    auto sol_result = builder.buildSolution(solution_dir, test_dir, job_id);

    if(!sol_result.success) {
        std::cerr << "[" << sol_label << "] SOLUTION BUILD FAILED: "
            << sol_result.error_message << "\n";
        std::cerr << sol_result.build_output << "\n";
        if(do_cleanup) builder.cleanup(sol_result.build_dir);
        return {0, 0};
    }

    std::cout << "[" << sol_label << "] Solution build OK: "
        << fs::path(sol_result.solution_lib_path).filename().string() << "\n";
    if(sol_result.cmake_replaced) {
        std::cerr << "[" << sol_label << "] WARNING: Solution CMakeLists.txt was replaced ("
            << sol_result.cmake_replaced_reason << ")\n";
    }

    // Step 2: Build test plugins against student DLL
    auto student_lib_dir = fs::path(sol_result.solution_lib_path).parent_path().string();
    auto test_result = builder.buildTests(test_dir, student_lib_dir, sol_result.solution_include_dir);

    if(!test_result.success) {
        std::cerr << "[" << sol_label << "] TEST BUILD FAILED: "
            << test_result.error_message << "\n";
        std::cerr << test_result.build_output << "\n";
        if(do_cleanup) {
            builder.cleanup(sol_result.build_dir);
            builder.cleanup(test_result.build_dir);
        }
        return {0, 0};
    }

    std::cout << "[" << sol_label << "] Test build OK (" << test_result.plugin_paths.size() << " plugins)\n";
    if(test_result.cmake_replaced) {
        std::cerr << "[" << sol_label << "] WARNING: Test CMakeLists.txt was replaced ("
            << test_result.cmake_replaced_reason << ")\n";
    }

    // Step 3: Load DLLs
    // Set fresh registry BEFORE loading DLLs so REGISTER_TEST goes here
    TestRegistry registry;
    TestRegistry::setActiveInstance(&registry);

    PluginLoader loader;

    // Load student solution FIRST (Windows resolves symbols from it)
    if(!sol_result.solution_lib_path.empty()) {
        if(!loader.loadPlugin(sol_result.solution_lib_path)) {
            std::cerr << "[" << sol_label << "] WARNING: Could not load student_solution lib\n";
        }
    }

    for(const auto& dep_path : test_result.dep_lib_paths) {
        loader.loadPlugin(dep_path);
    }

    int plugins_failed = 0;
    for(const auto& plugin_path : test_result.plugin_paths) {
        if(!loader.loadPlugin(plugin_path)) {
            plugins_failed++;
        }
    }

    if(plugins_failed > 0) {
        std::cerr << "[" << sol_label << "] ERROR: " << plugins_failed << " of "
            << test_result.plugin_paths.size()
            << " test plugin(s) failed to load. "
            << "This usually means the student solution does not export "
            << "the required symbols (check that function signatures in "
            << "the solution match the test interface headers).\n";
    }

    if(registry.size() == 0) {
        std::cerr << "[" << sol_label << "] ERROR: No test scenarios registered\n";

        nlohmann::json report;
        report["solution"] = sol_label;
        report["status"] = to_string(job_status::failed);
        if(plugins_failed > 0) {
            report["error"] = std::to_string(plugins_failed) + " of "
                + std::to_string(test_result.plugin_paths.size())
                + " test plugin(s) failed to load. "
                "The student solution likely does not export the required symbols "
                "(function signatures do not match the test interface headers).";
        } else {
            report["error"] = "No test scenarios registered";
        }
        if(test_result.cmake_replaced) {
            report["buildInfo"]["testCmakeReplaced"] = true;
            report["buildInfo"]["testCmakeReplacedReason"] = test_result.cmake_replaced_reason;
        }
        if(sol_result.cmake_replaced) {
            report["buildInfo"]["solutionCmakeReplaced"] = true;
            report["buildInfo"]["solutionCmakeReplacedReason"] = sol_result.cmake_replaced_reason;
        }
        std::cout << "\n" << report.dump(4) << "\n";

        loader.unloadAll();
        TestRegistry::clearActiveInstance();
        if(do_cleanup) {
            builder.cleanup(sol_result.build_dir);
            builder.cleanup(test_result.build_dir);
        }
        return {0, 0};
    }

    std::cout << "[" << sol_label << "] Scenarios: " << registry.size() << "\n";

    // Run tests
    auto run_mode = [&](const std::string& mode_name, par::Mode monitor_mode, ScenarioType type) {
        auto ctx = par::monitor::createContext();
        ctx->mode = monitor_mode;
        ctx->max_threads = threads;
        par::monitor::activateContext(ctx.get());

        TestEngine engine;
        auto tc = ThreadCounts::get(mode_name, threads);
        auto res = engine.execute(tc, registry, *ctx, type);

        par::monitor::activateContext(nullptr);
        return std::make_pair(res, tc);
    };

    nlohmann::json report;
    report["solution"] = fs::path(solution_dir).filename().string();
    report["mode"] = mode;
    {
        nlohmann::json build_info;
        if(test_result.cmake_replaced) {
            build_info["testCmakeReplaced"] = true;
            build_info["testCmakeReplacedReason"] = test_result.cmake_replaced_reason;
        }
        if(sol_result.cmake_replaced) {
            build_info["solutionCmakeReplaced"] = true;
            build_info["solutionCmakeReplacedReason"] = sol_result.cmake_replaced_reason;
        }
        if(!build_info.empty()) {
            report["buildInfo"] = build_info;
        }
    }

    std::vector<TestScenarioResult> all_results;
    bool correctness_passed = true;

    if(mode == "correctness" || mode == "all") {
        auto [results, tc] = run_mode("correctness", par::Mode::STRESS, ScenarioType::CORRECTNESS);
        report["correctness"] = TestScenarioResultConverter::to_grouped_json(results, tc, false);
        for(const auto& sr : results) {
            for(const auto& tr : sr.results) {
                if(!tr.passed) correctness_passed = false;
            }
        }
        all_results.insert(
            all_results.end(),
            std::make_move_iterator(results.begin()),
            std::make_move_iterator(results.end())
        );
    }
    if(mode == "performance" || (mode == "all" && correctness_passed)) {
        auto [results, tc] = run_mode("performance", par::Mode::MONITOR, ScenarioType::PERFORMANCE);
        report["performance"] = TestScenarioResultConverter::to_grouped_json(results, tc, true);
        all_results.insert(
            all_results.end(),
            std::make_move_iterator(results.begin()),
            std::make_move_iterator(results.end())
        );
    } else if(mode == "all" && !correctness_passed) {
        std::cout << "[" << sol_label << "] Correctness failed -> performance skipped\n";
        report["performanceSkipped"] = true;
    }

    // Results
    std::cout << "\n" << report.dump(4) << "\n";

    int total = 0, passed = 0;
    for(const auto& r : all_results) {
        for(const auto& t : r.results) {
            total++;
            if(t.passed) passed++;
        }
    }

    std::cout << "[" << sol_label << "] " << passed << " / " << total << " passed\n";

    // Cleanup: unload DLLs while registry is still alive (prevents dangling vtables)
    par::monitor::activateContext(nullptr);
    loader.unloadAll();
    TestRegistry::clearActiveInstance();

    if(do_cleanup) {
        builder.cleanup(sol_result.build_dir);
        builder.cleanup(test_result.build_dir);
    }

    // registry destroyed here — safe because unloadAll() already cleared it
    return {total, passed};
}

int main(int argc, char** argv) {
    std::vector<std::string> solution_dirs;
    std::string test_dir;
    std::string test_id;
    std::string engine_lib;
    std::string engine_include;
    std::string parallel_lib_path;
    std::string parallel_include;
    std::string mode = "all";
    int threads = 4;
    bool do_cleanup = true;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--solution" && i + 1 < argc) solution_dirs.push_back(argv[++i]);
        else if(arg == "--test-dir" && i + 1 < argc) test_dir = argv[++i];
        else if(arg == "--test-id" && i + 1 < argc) test_id = argv[++i];
        else if(arg == "--engine-lib" && i + 1 < argc) engine_lib = argv[++i];
        else if(arg == "--engine-include" && i + 1 < argc) engine_include = argv[++i];
        else if(arg == "--parallel-lib" && i + 1 < argc) parallel_lib_path = argv[++i];
        else if(arg == "--parallel-include" && i + 1 < argc) parallel_include = argv[++i];
        else if(arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else if(arg == "--threads" && i + 1 < argc) threads = std::atoi(argv[++i]);
        else if(arg == "--no-cleanup") do_cleanup = false;
        else if(arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Auto-detect paths from project root
    fs::path project_root = project::findRoot(fs::path(argv[0]));
    if(project_root.empty()) {
        project_root = fs::current_path();
    }

    if(solution_dirs.empty()) solution_dirs.push_back((project_root / "solution").string());
    if(engine_include.empty()) engine_include = (project_root / "test_engine" / "src").string();
    if(parallel_include.empty()) parallel_include = (project_root / "parallel_lib" / "include").string();

    #ifdef _WIN32
    if(engine_lib.empty()) engine_lib = findLib(project_root, "libtest_engine.dll");
    if(parallel_lib_path.empty()) parallel_lib_path = findLib(project_root, "libparallel_lib.dll");
    #else
    if(engine_lib.empty()) engine_lib = findLib(project_root, "libtest_engine.so");
    if(parallel_lib_path.empty()) parallel_lib_path = findLib(project_root, "libparallel_lib.so");
    #endif

    if(test_id.empty()) {
        std::cerr << "ERROR: --test-id is required\n";
        printUsage(argv[0]);
        return 1;
    }
    if(test_dir.empty()) {
        std::cerr << "ERROR: --test-dir is required\n";
        printUsage(argv[0]);
        return 1;
    }

    // Validate paths
    if(!fs::is_directory(test_dir)) {
        std::cerr << "ERROR: Test directory not found: " << test_dir << "\n";
        return 1;
    }
    if(engine_lib.empty() || !fs::exists(engine_lib)) {
        std::cerr << "ERROR: Engine library not found. Use --engine-lib <path>\n";
        return 1;
    }
    if(parallel_lib_path.empty() || !fs::exists(parallel_lib_path)) {
        std::cerr << "ERROR: Parallel library not found. Use --parallel-lib <path>\n";
        return 1;
    }

    std::cout << "============================================\n"
        << "  Server Pipeline Integration Test\n"
        << "============================================\n"
        << "Test ID:           " << test_id << "\n"
        << "Test dir:          " << test_dir << "\n"
        << "Solutions:         " << solution_dirs.size() << "\n";
    for(size_t i = 0; i < solution_dirs.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << solution_dirs[i] << "\n";
    }
    std::cout << "Mode:              " << mode << "\n"
        << "Threads:           " << threads << "\n"
        << "============================================\n";

    // Setup BuildService
    BuildService::BuildConfig config;
    config.engine_lib_path = engine_lib;
    config.engine_include_path = engine_include;
    config.parallel_lib_path = parallel_lib_path;
    config.parallel_include_path = parallel_include;
    config.cmake_executable = "cmake";
    config.exe_dir = fs::path(argv[0]).parent_path().string();
    #ifdef _WIN32
    config.generator = "MinGW Makefiles";
    #else
    config.generator = "Ninja";
    #endif

    BuildService builder(config);

    // Run each solution (solution build + test build per solution)
    int grand_total = 0, grand_passed = 0;
    int solutions_ok = 0;

    for(const auto& sol_dir : solution_dirs) {
        if(!fs::is_directory(sol_dir)) {
            std::cerr << "\nERROR: Solution directory not found: " << sol_dir << " (skipping)\n";
            continue;
        }

        auto [total, passed] = runSolution(
            sol_dir,
            test_dir,
            mode,
            threads,
            do_cleanup,
            builder
        );

        grand_total += total;
        grand_passed += passed;
        if(total > 0 && passed == total) solutions_ok++;
    }

    // Final summary
    std::cout << "\n============================================\n"
        << "  FINAL SUMMARY\n"
        << "============================================\n"
        << "  Solutions:  " << solutions_ok << " / " << solution_dirs.size() << " all-pass\n"
        << "  Tests:      " << grand_passed << " / " << grand_total << " passed\n"
        << "============================================\n";

    std::cout << "\nDone.\n";

    return (grand_passed == grand_total && grand_total > 0) ? 0 : 1;
}