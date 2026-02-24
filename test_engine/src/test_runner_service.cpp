#include <filesystem>
#include <iostream>
#include <numa_utils.h>
#include <plugin_loader.h>
#include <process_utils.h>
#include <test_engine.h>
#include <test_runner_service.h>
#include <test_registry.h>
#include <test_scenario_result_converter.h>
#include <thread_counts.h>
#include <par/monitor.h>

namespace fs = std::filesystem;

namespace {

std::string cloneRepo(
    const std::string& git_url,
    const std::string& branch,
    const fs::path& dest
) {
    std::string cmd = "git clone --depth 1";
    if(!branch.empty()) {
        cmd += " --branch \"" + branch + "\"";
    }
    cmd += " \"" + git_url + "\" \"" + dest.string() + "\" 2>&1";
    #ifdef _WIN32
    cmd = "\"" + cmd + "\"";
    #endif
    auto result = runCommand(cmd);
    return result.output;
}

fs::path resolvePath(const std::string& p, const fs::path& base_dir) {
    if(p.empty()) return {};
    fs::path fp(p);
    if(fp.is_relative() && !base_dir.empty())
        fp = base_dir / fp;
    return fs::weakly_canonical(fp);
}

void cleanupTempDir(const fs::path& temp_dir) {
    if(!temp_dir.empty()) {
        std::error_code ec;
        fs::remove_all(temp_dir, ec);
    }
}

struct scope_cleanup {
    std::function<void()> fn;
    ~scope_cleanup() { if(fn) fn(); }
};

} // anonymous namespace

TestRunnerService::TestRunnerService(BuildService::BuildConfig config)
    : exe_dir_(config.exe_dir),
      correctness_workers_(config.correctness_workers),
      build_service_(std::move(config)) {
    queue_ = std::make_unique<JobQueue>(
        [this](const nlohmann::json& req, std::function<void(JobQueue::JobStatus)> updater) {
            return execute(req, std::move(updater));
        },
        correctness_workers_
    );
}

std::string TestRunnerService::submit(nlohmann::json request, CompletionCallback on_complete) {
    return queue_->submit(std::move(request), std::move(on_complete));
}

JobQueue::JobInfo TestRunnerService::getJobInfo(const std::string& job_id) const {
    return queue_->getJobInfo(job_id);
}

nlohmann::json TestRunnerService::getQueueStatus() const {
    return queue_->getStatus();
}

bool TestRunnerService::cancel(const std::string& job_id) {
    return queue_->cancel(job_id);
}

void TestRunnerService::setMaxCorrectnessWorkers(int n) {
    queue_->resizeCorrectnessPool(n);
}

void TestRunnerService::setJobRetentionSeconds(int sec) {
    queue_->setJobRetentionSeconds(sec);
}

// ============================================================================
// Path resolution (local dirs or git clone)
// ============================================================================

TestRunnerService::ResolvedPaths TestRunnerService::resolvePaths(
    const nlohmann::json& request, const std::string& job_id
) {
    ResolvedPaths paths;

    paths.test_dir = resolvePath(request.value("testDir", ""), exe_dir_).string();

    if(paths.test_dir.empty() && request.contains("testGitUrl")) {
        paths.temp_clone_dir = fs::temp_directory_path() / ("clone-" + job_id);
        std::error_code ec;
        fs::remove_all(paths.temp_clone_dir, ec);
        fs::create_directories(paths.temp_clone_dir);

        auto tests_path = paths.temp_clone_dir / "tests";
        std::string out = cloneRepo(
            request["testGitUrl"].get<std::string>(),
            request.value("testBranch", ""),
            tests_path
        );
        if(!fs::exists(tests_path))
            throw std::runtime_error("Failed to clone test dir: " + out);
        paths.test_dir = tests_path.string();
    }

    paths.solution_dir = resolvePath(request.value("solutionDir", ""), exe_dir_).string();

    if(paths.solution_dir.empty() && request.contains("solutionGitUrl")) {
        if(paths.temp_clone_dir.empty()) {
            paths.temp_clone_dir = fs::temp_directory_path() / ("clone-" + job_id);
            std::error_code ec;
            fs::remove_all(paths.temp_clone_dir, ec);
            fs::create_directories(paths.temp_clone_dir);
        }
        auto sol_path = paths.temp_clone_dir / "solution";
        std::string out = cloneRepo(
            request["solutionGitUrl"].get<std::string>(),
            request.value("solutionBranch", ""),
            sol_path
        );
        if(!fs::exists(sol_path))
            throw std::runtime_error("Failed to clone solution: " + out);
        paths.solution_dir = sol_path.string();
    }

    if(paths.test_dir.empty())
        throw std::runtime_error("Missing test_dir or test_git_url");
    if(paths.solution_dir.empty())
        throw std::runtime_error("Missing solution_dir or solution_git_url");

    return paths;
}

// ============================================================================
// Build info formatting
// ============================================================================

nlohmann::json TestRunnerService::formatBuildInfo(
    const BuildService::SolutionBuildResult& sol,
    const BuildService::TestBuildResult& test,
    const std::string& plugin_load_error
) {
    nlohmann::json build_info;
    if(test.cmake_replaced) {
        build_info["testCmakeReplaced"] = true;
        build_info["testCmakeReplacedReason"] = test.cmake_replaced_reason;
    }
    if(sol.cmake_replaced) {
        build_info["solutionCmakeReplaced"] = true;
        build_info["solutionCmakeReplacedReason"] = sol.cmake_replaced_reason;
    }
    if(!plugin_load_error.empty()) {
        build_info["pluginLoadError"] = plugin_load_error;
    }
    return build_info;
}

// ============================================================================
// Plugin loading — student solution + test DLLs in correct order
// ============================================================================

TestRunnerService::loaded_plugins TestRunnerService::loadPlugins(
    const BuildService::SolutionBuildResult& sol_result,
    const BuildService::TestBuildResult& test_result
) {
    loaded_plugins lp;
    lp.plugins_total = static_cast<int>(test_result.plugin_paths.size());

    // First: load REAL student solution (so Windows resolves symbols from it)
    if(!sol_result.solution_lib_path.empty()) {
        if(!lp.loader.loadPlugin(sol_result.solution_lib_path)) {
            throw std::runtime_error(
                "Failed to load student solution: " +
                sol_result.solution_lib_path
            );
        }
    }

    // Then: load test dependency libraries
    for(const auto& dep_path : test_result.dep_lib_paths) {
        lp.loader.loadPlugin(dep_path);
    }

    // Finally: load test plugins (they'll resolve student_solution symbols
    // from the already-loaded real DLL)
    for(const auto& plugin_path : test_result.plugin_paths) {
        if(!lp.loader.loadPlugin(plugin_path)) {
            lp.plugins_failed++;
        }
    }

    if(lp.plugins_failed > 0) {
        lp.load_error = std::to_string(lp.plugins_failed) + " of "
            + std::to_string(lp.plugins_total)
            + " test plugin(s) failed to load. "
            "The student solution likely does not export the required symbols "
            "(function signatures do not match the test interface headers).";
        std::cerr << "[TestRunner] " << lp.load_error << "\n";
    }

    return lp;
}

// ============================================================================
// Test execution — NUMA pin + run + assemble JSON
// ============================================================================

void TestRunnerService::runTests(
    nlohmann::json& result,
    const std::string& job_id,
    const std::string& mode,
    int threads, int numa_node,
    TestRegistry& registry,
    par::MonitorContext& ctx,
    const std::string& plugin_load_error
) {
    // NUMA pin if requested
    if(numa_node >= 0) {
        numa::pinToNode(numa_node);
        numa::setMemoryPolicy(numa_node);
    }

    TestEngine engine;

    if(mode == "all") {
        // === Mode "all": correctness first, then performance if all passed ===

        std::cout << "[TestRunner] " << job_id << " | Running correctness tests (STRESS)...\n";
        auto correctness_counts = ThreadCounts::get("correctness", threads);
        auto correctness_results = engine.execute(
            correctness_counts, registry, ctx, ScenarioType::CORRECTNESS
        );

        result["correctness"] = TestScenarioResultConverter::to_grouped_json(
            correctness_results, correctness_counts, false, numa_node
        );

        // Check if ALL correctness tests passed
        bool all_passed = true;
        std::string fail_reason;
        for(const auto& sr : correctness_results) {
            for(const auto& tr : sr.results) {
                if(!tr.passed) {
                    all_passed = false;
                    if(fail_reason.empty()) {
                        fail_reason = sr.name + "/" + tr.name + ": " + tr.message;
                    }
                }
            }
        }

        if(all_passed && plugin_load_error.empty()) {
            std::cout << "[TestRunner] " << job_id
                << " | Correctness passed -> running performance tests (MONITOR)...\n";
            ctx.mode = par::Mode::MONITOR;
            ctx.max_threads = threads;

            auto perf_counts = ThreadCounts::get("performance", threads);
            auto perf_results = engine.execute(
                perf_counts, registry, ctx, ScenarioType::PERFORMANCE
            );

            result["performance"] = TestScenarioResultConverter::to_grouped_json(
                perf_results, perf_counts, true, numa_node
            );
            result["status"] = "completed";
        } else {
            std::cout << "[TestRunner] " << job_id
                << " | Correctness failed -> performance skipped\n";
            result["performanceSkipped"] = true;
            result["performanceSkipReason"] = plugin_load_error.empty()
                ? ("Correctness tests failed: " + fail_reason)
                : plugin_load_error;
            result["status"] = "failed";
            result["error"] = plugin_load_error.empty()
                ? "Correctness tests failed" : plugin_load_error;
        }
    } else {
        // === Single mode: correctness or performance ===

        std::cout << "[TestRunner] " << job_id << " | Running " << mode
            << " tests (threads=" << threads << ")...\n";
        auto thread_counts = ThreadCounts::get(mode, threads);
        ScenarioType type = (mode == "performance")
            ? ScenarioType::PERFORMANCE
            : ScenarioType::CORRECTNESS;
        auto test_results = engine.execute(thread_counts, registry, ctx, type);

        result["status"] = plugin_load_error.empty() ? "completed" : "failed";
        if(!plugin_load_error.empty()) {
            result["error"] = plugin_load_error;
        }

        result[mode] = TestScenarioResultConverter::to_grouped_json(
            test_results, thread_counts, mode == "performance", numa_node
        );
    }
}

// ============================================================================
// Main entry point: solution built first, then tests compiled against it
// ============================================================================

nlohmann::json TestRunnerService::execute(
    const nlohmann::json& request,
    std::function<void(JobQueue::JobStatus)> status_updater
) {
    std::string job_id = request.at("jobId").get<std::string>();
    std::string test_id = request.at("testId").get<std::string>();
    std::string mode = request.value("mode", "correctness");
    int threads = request.value("threads", 4);
    int numa_node = request.value("numaNode", -1);

    auto paths = resolvePaths(request, job_id);

    std::string solution_name = fs::path(paths.solution_dir).filename().string();
    std::cout << "[TestRunner] " << job_id << " | mode=" << mode
        << " | solution=" << solution_name
        << " | test_id=" << test_id
        << " | threads=" << threads << "\n";

    nlohmann::json result;
    result["jobId"] = job_id;
    result["solution"] = solution_name;
    result["mode"] = mode;

    auto monitor_ctx = par::monitor::createContext();
    monitor_ctx->mode = (mode == "performance") ? par::Mode::MONITOR : par::Mode::STRESS;
    monitor_ctx->max_threads = threads;
    par::monitor::activateContext(monitor_ctx.get());

    TestRegistry registry;
    TestRegistry::setActiveInstance(&registry);

    std::string solution_build_dir;
    std::string test_build_dir;

    scope_cleanup cleanup{
        [&]() {
            TestRegistry::clearActiveInstance();
            par::monitor::activateContext(nullptr);
            if(!solution_build_dir.empty()) build_service_.cleanup(solution_build_dir);
            if(!test_build_dir.empty()) build_service_.cleanup(test_build_dir);
            cleanupTempDir(paths.temp_clone_dir);
            numa::reset();
        }
    };

    // Step 1: Build student solution (internet blocked)
    std::cout << "[TestRunner] " << job_id << " | Step 1: building solution...\n";
    status_updater(JobQueue::JobStatus::BUILDING);
    auto sol_result = build_service_.buildSolution(paths.solution_dir, paths.test_dir, job_id);
    solution_build_dir = sol_result.build_dir;

    if(!sol_result.success) {
        result["status"] = "failed";
        result["error"] = "Solution build failed: " + sol_result.error_message;
        result["buildOutput"] = sol_result.build_output;
        auto build_info = formatBuildInfo(sol_result, BuildService::TestBuildResult{}, "");
        if(!build_info.empty()) result["buildInfo"] = build_info;
        throw std::runtime_error(result["error"].get<std::string>());
    }

    // Step 2: Build test plugins against real student DLL (internet allowed)
    std::cout << "[TestRunner] " << job_id << " | Step 2: building test plugins...\n";
    auto student_lib_dir = fs::path(sol_result.solution_lib_path).parent_path().string();
    auto test_result = build_service_.buildTests(paths.test_dir, student_lib_dir, sol_result.solution_include_dir);
    test_build_dir = test_result.build_dir;

    if(!test_result.success) {
        result["status"] = "failed";
        result["error"] = "Test build failed: " + test_result.error_message;
        result["buildOutput"] = test_result.build_output;
        auto build_info = formatBuildInfo(sol_result, test_result, "");
        if(!build_info.empty()) result["buildInfo"] = build_info;
        throw std::runtime_error(result["error"].get<std::string>());
    }

    // Step 3: Load DLLs in correct order
    std::cout << "[TestRunner] " << job_id << " | Step 3: loading "
        << test_result.plugin_paths.size() << " plugin(s)...\n";
    status_updater(JobQueue::JobStatus::RUNNING);
    auto lp = loadPlugins(sol_result, test_result);

    // If ALL plugins failed — no point running tests
    if(lp.plugins_failed > 0 && lp.plugins_failed == lp.plugins_total) {
        result["status"] = "failed";
        result["error"] = lp.load_error;
        result["buildInfo"] = formatBuildInfo(sol_result, test_result, lp.load_error);
        lp.loader.unloadAll();
        throw std::runtime_error(lp.load_error);
    }

    // Step 4-6: NUMA pin + run tests + assemble JSON
    runTests(result, job_id, mode, threads, numa_node, registry, *monitor_ctx, lp.load_error);

    auto build_info = formatBuildInfo(sol_result, test_result, lp.load_error);
    if(!build_info.empty()) result["buildInfo"] = build_info;

    // Step 7: Unload plugins
    lp.loader.unloadAll();

    // Step 8: Cleanup handled by scope_cleanup RAII guard
    return result;
}
