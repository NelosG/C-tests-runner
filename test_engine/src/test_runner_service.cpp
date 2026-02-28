#include <filesystem>
#include <iostream>
#include <numa_utils.h>
#include <plugin_loader.h>
#include <test_engine.h>
#include <test_runner_service.h>
#include <test_registry.h>
#include <test_scenario_result_converter.h>
#include <thread_counts.h>
#include <par/monitor.h>
#include <par/memory_guard.h>

namespace fs = std::filesystem;


namespace {

    struct scope_cleanup {
        std::function<void()> fn;
        ~scope_cleanup() { if(fn) fn(); }
    };

} // anonymous namespace

TestRunnerService::TestRunnerService(
    BuildService::BuildConfig config,
    ResourceManager& resource_manager
)
    : resource_manager_(resource_manager),
      exe_dir_(config.exe_dir),
      correctness_workers_(config.correctness_workers),
      default_memory_limit_mb_(config.default_memory_limit_mb),
      build_service_(std::move(config)) {
    queue_ = std::make_unique<JobQueue>(
        [this](const nlohmann::json& req, std::function<void(job_status)> updater) {
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
// Path resolution via ResourceManager
// ============================================================================

TestRunnerService::ResolvedPaths TestRunnerService::resolvePaths(const nlohmann::json& request) {
    ResolvedPaths paths;

    // Resolve test source
    std::string test_type = request.value("testSourceType", "local");
    nlohmann::json test_desc = request.value("testSource", nlohmann::json::object());
    test_desc["_kind"] = "test";  // hint for LocalResourceProvider base dir selection
    paths.test_dir = resource_manager_.resolve(test_type, test_desc).string();

    // Resolve solution source
    std::string sol_type = request.value("solutionSourceType", "local");
    nlohmann::json sol_desc = request.value("solutionSource", nlohmann::json::object());
    sol_desc["_kind"] = "solution";
    paths.solution_dir = resource_manager_.resolve(sol_type, sol_desc).string();

    if(paths.test_dir.empty())
        throw std::runtime_error("testSource resolved to empty path");
    if(paths.solution_dir.empty())
        throw std::runtime_error("solutionSource resolved to empty path");

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
    int threads,
    int numa_node,
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

    if(mode == to_string(test_mode::all)) {
        // === Mode "all": correctness first, then performance if all passed ===

        std::cout << "[TestRunner] " << job_id << " | Running correctness tests (STRESS)...\n";
        auto correctness_counts = ThreadCounts::get(to_string(test_mode::correctness), threads);
        auto correctness_results = engine.execute(
            correctness_counts,
            registry,
            ctx,
            ScenarioType::CORRECTNESS
        );

        result[to_string(test_mode::correctness)] = TestScenarioResultConverter::to_grouped_json(
            correctness_results,
            correctness_counts,
            false,
            numa_node
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

            auto perf_counts = ThreadCounts::get(to_string(test_mode::performance), threads);
            auto perf_results = engine.execute(
                perf_counts,
                registry,
                ctx,
                ScenarioType::PERFORMANCE
            );

            result[to_string(test_mode::performance)] = TestScenarioResultConverter::to_grouped_json(
                perf_results,
                perf_counts,
                true,
                numa_node
            );
            result["status"] = to_string(job_status::completed);
        } else {
            std::cout << "[TestRunner] " << job_id
                << " | Correctness failed -> performance skipped\n";
            result["performanceSkipped"] = true;
            result["performanceSkipReason"] = plugin_load_error.empty()
                ? ("Correctness tests failed: " + fail_reason)
                : plugin_load_error;
            result["status"] = to_string(job_status::failed);
            result["error"] = plugin_load_error.empty()
                ? "Correctness tests failed"
                : plugin_load_error;
        }
    } else {
        // === Single mode: correctness or performance ===

        std::cout << "[TestRunner] " << job_id << " | Running " << mode
            << " tests (threads=" << threads << ")...\n";
        auto thread_counts = ThreadCounts::get(mode, threads);
        ScenarioType type = (mode == to_string(test_mode::performance))
            ? ScenarioType::PERFORMANCE
            : ScenarioType::CORRECTNESS;
        auto test_results = engine.execute(thread_counts, registry, ctx, type);

        result["status"] = to_string(plugin_load_error.empty() ? job_status::completed : job_status::failed);
        if(!plugin_load_error.empty()) {
            result["error"] = plugin_load_error;
        }

        result[mode] = TestScenarioResultConverter::to_grouped_json(
            test_results,
            thread_counts,
            mode == to_string(test_mode::performance),
            numa_node
        );
    }
}

// ============================================================================
// Main entry point: solution built first, then tests compiled against it
// ============================================================================

nlohmann::json TestRunnerService::execute(
    const nlohmann::json& request,
    std::function<void(job_status)> status_updater
) {
    std::string job_id = request.at("jobId").get<std::string>();
    std::string test_id = request.at("testId").get<std::string>();
    std::string mode = request.value("mode", to_string(test_mode::correctness));
    int threads = request.value("threads", 4);
    int numa_node = request.value("numaNode", -1);

    // Step 0: Resolve source paths via ResourceManager
    auto paths = resolvePaths(request);

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
    monitor_ctx->mode = (mode == to_string(test_mode::performance)) ? par::Mode::MONITOR : par::Mode::STRESS;
    monitor_ctx->max_threads = threads;
    par::monitor::activateContext(monitor_ctx.get());

    // Memory limiting — per-job context from request or server default
    long long memory_limit_mb = request.value("memoryLimitMb",
        default_memory_limit_mb_);
    long long memory_limit_bytes = memory_limit_mb > 0 ? memory_limit_mb * 1024LL * 1024LL : 0;
    auto memory_ctx = par::memory::createContext(memory_limit_bytes);
    par::memory::activateContext(memory_ctx.get());

    TestRegistry registry;
    TestRegistry::setActiveInstance(&registry);

    std::string solution_build_dir;
    std::string test_build_dir;

    scope_cleanup cleanup{
        [&]() {
            TestRegistry::clearActiveInstance();
            par::monitor::activateContext(nullptr);
            par::memory::activateContext(nullptr);
            if(!solution_build_dir.empty()) build_service_.cleanup(solution_build_dir);
            if(!test_build_dir.empty()) build_service_.cleanup(test_build_dir);
            numa::reset();
        }
    };

    // Step 1: Build student solution (internet blocked)
    std::cout << "[TestRunner] " << job_id << " | Step 1: building solution...\n";
    status_updater(job_status::building);
    auto sol_result = build_service_.buildSolution(paths.solution_dir, paths.test_dir, job_id);
    solution_build_dir = sol_result.build_dir;

    if(!sol_result.success) {
        result["status"] = to_string(job_status::failed);
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
        result["status"] = to_string(job_status::failed);
        result["error"] = "Test build failed: " + test_result.error_message;
        result["buildOutput"] = test_result.build_output;
        auto build_info = formatBuildInfo(sol_result, test_result, "");
        if(!build_info.empty()) result["buildInfo"] = build_info;
        throw std::runtime_error(result["error"].get<std::string>());
    }

    // Step 3: Load DLLs in correct order
    std::cout << "[TestRunner] " << job_id << " | Step 3: loading "
        << test_result.plugin_paths.size() << " plugin(s)...\n";
    status_updater(job_status::running);
    auto lp = loadPlugins(sol_result, test_result);

    // If ALL plugins failed — no point running tests
    if(lp.plugins_failed > 0 && lp.plugins_failed == lp.plugins_total) {
        result["status"] = to_string(job_status::failed);
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