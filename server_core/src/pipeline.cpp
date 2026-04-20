#include "pipeline.h"

#include <cctype>
#include <cmake_validator.h>
#include <framework_detector.h>
#include <iostream>
#include <json_scenario_loader.h>
#include <log_utils.h>
#include <path_sanitizer.h>
#include <request_helpers.h>
#include <scope_cleanup.h>
#include <test_registry.h>
#include <test_scenario_result_converter.h>
#include <thread>
#include <thread_counts.h>
#include <time_utils.h>

namespace fs = std::filesystem;

namespace {

    inline std::string sanitize(const std::string& s) { return path_sanitizer::sanitize(s); }

    /// Aggregate non-fatal build diagnostics into a single JSON object.
    /// Currently the only such field is pluginLoadError (failed-to-dlopen plugin DLLs).
    nlohmann::json format_build_info(const std::string& plugin_load_error) {
        nlohmann::json info;
        if(!plugin_load_error.empty()) info["pluginLoadError"] = sanitize(plugin_load_error);
        return info;
    }

} // namespace

// ============================================================================
// JobContext helpers
// ============================================================================

void Pipeline::JobContext::add_step(
    const std::string& name, const std::string& status, double ms
) {
    pipeline.push_back({{"step", name}, {"status", status}, {"durationMs", ms}});
}

double Pipeline::JobContext::step_ms() {
    auto now = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - step_start).count();
    step_start = now;
    return ms;
}

void Pipeline::JobContext::emit_phase(const std::string& phase,
                                      const std::string& message,
                                      std::optional<double> progress) const {
    if(!on_progress) return;
    nlohmann::json event = {
        {"jobId",     job_id},
        {"nodeId",    node_id},
        {"phase",     phase},
        {"timestamp", now_iso8601()}
    };
    if(!message.empty()) event["message"] = message;
    if(progress.has_value()) event["progress"] = *progress;
    try { on_progress(event); } catch(...) {
        // Best-effort: never let progress emission break the pipeline.
    }
}

// ============================================================================
// failStep - uniform failure path for pipeline steps
// ============================================================================

bool Pipeline::fail_step(JobContext& ctx, const std::string& step,
                        const std::string& error_message,
                        nlohmann::json extra_details) {
    ctx.add_step(step, "failed", ctx.step_ms());
    ctx.result["status"]     = to_string(job_status::failed);
    ctx.result["failedStep"] = step;
    ctx.result["error"]      = error_message;
    // Accept only object payloads here - assigning to operator[] on non-object
    // non-null JSON throws type_error. This is defensive against future callers.
    nlohmann::json details = extra_details.is_object() ? std::move(extra_details)
                                                       : nlohmann::json::object();
    details["step"] = step;
    ctx.result["errorDetails"] = std::move(details);
    return false;
}

// ============================================================================
// Construction
// ============================================================================

Pipeline::Pipeline(BuildService& build_service,
                   SandboxTestExecutor& test_executor,
                   ResourceManager& resource_manager,
                   LaneController& lane_controller)
    : build_service_(build_service),
      test_executor_(test_executor),
      resource_manager_(resource_manager),
      lane_controller_(lane_controller) {}

void Pipeline::init_job_context(JobContext& ctx) const {
    ctx.job_id           = ctx.request.at("jobId").get<std::string>();
    ctx.test_id          = ctx.request.at("testId").get<std::string>();
    // node_id is injected by the adapter as "_node_id" so progress events can
    // be tagged. Falls back to empty string for non-adapter callers (CLI).
    ctx.node_id          = ctx.request.value("_node_id", "");
    // Read LIVE atomics - these may be mutated concurrently by apply_config().
    const long long mem_default     = build_service_.default_memory_limit_mb();
    const int       threads_default = build_service_.default_threads();
    const int       wall_default    = build_service_.default_wall_time_sec();
    const int       cpu_default     = build_service_.default_cpu_time_sec();
    const int       proc_multiplier = build_service_.sandbox_process_multiplier();
    ctx.threads          = ctx.request.value("threads", threads_default);
    ctx.memory_limit_mb  = ctx.request.value("memoryLimitMb", mem_default);
    ctx.wall_time_sec    = ctx.request.value("wallTimeSec", wall_default);
    ctx.cpu_time_sec     = ctx.request.value("cpuTimeSec", cpu_default);
    ctx.max_processes    = ctx.request.value("maxProcesses",
                                             ctx.threads * proc_multiplier);
    ctx.solution_name    = request_helpers::extract_solution_name(ctx.request);

    ctx.result["jobId"]    = ctx.job_id;
    ctx.result["solution"] = ctx.solution_name;
    ctx.result["effectiveParams"] = {
        {"threads", ctx.threads},
        {"memoryLimitMb", ctx.memory_limit_mb},
        {"wallTimeSec", ctx.wall_time_sec},
        {"cpuTimeSec", ctx.cpu_time_sec},
        {"maxProcesses", ctx.max_processes},
        {"testSourceType", ctx.request.value("testSourceType", "local")},
        {"solutionSourceType", ctx.request.value("solutionSourceType", "local")}
    };
    ctx.result["environment"] = {
        {"hardwareThreads", static_cast<int>(std::thread::hardware_concurrency())},
        #ifdef _WIN32
        {"platform", "windows"},
        #else
        {"platform", "linux"},
        #endif
    };
}

// ============================================================================
// resolvePaths
// ============================================================================

Pipeline::ResolvedPaths Pipeline::resolve_paths(
    const nlohmann::json& request,
    const JobContext& ctx
) const {
    ResolvedPaths paths;

    ctx.emit_phase("resolveTests");
    std::string test_type = request.value("testSourceType", "local");
    nlohmann::json test_desc = request.value("testSource", nlohmann::json::object());
    test_desc["_kind"] = "test";
    paths.test_dir = resource_manager_.resolve(test_type, test_desc).string();

    ctx.emit_phase("resolveSolution");
    std::string sol_type = request.value("solutionSourceType", "local");
    nlohmann::json sol_desc = request.value("solutionSource", nlohmann::json::object());
    sol_desc["_kind"] = "solution";
    paths.solution_dir = resource_manager_.resolve(sol_type, sol_desc).string();

    if(paths.test_dir.empty())     throw std::runtime_error("testSource resolved to empty path");
    if(paths.solution_dir.empty()) throw std::runtime_error("solutionSource resolved to empty path");
    return paths;
}

// ============================================================================
// Pipeline steps
// ============================================================================

bool Pipeline::step_resolve(JobContext& ctx) const {
    try {
        ctx.paths = resolve_paths(ctx.request, ctx);
    } catch(const std::exception& e) {
        return fail_step(ctx, "resolve", sanitize(e.what()), {
            {"testSourceType", ctx.request.value("testSourceType", "local")},
            {"solutionSourceType", ctx.request.value("solutionSourceType", "local")}
        });
    }
    ctx.add_step("resolve", "ok", ctx.step_ms());

    if(ctx.solution_name.empty()) {
        ctx.solution_name = fs::path(ctx.paths.solution_dir).filename().string();
        ctx.result["solution"] = ctx.solution_name;
    }
    LOG("Pipeline") << ctx.job_id
        << " | solution=" << ctx.solution_name
        << " | test_id=" << ctx.test_id
        << " | threads=" << ctx.threads << "\n";
    return true;
}

void Pipeline::step_parse_config(JobContext& ctx) const {
    ctx.emit_phase("parseConfig");
    ctx.assignment_config = assignment_config::load(ctx.paths.test_dir);

    // Empty allowedFrameworks is meaningful: assignment forbids any parallel
    // framework (sequential / std::thread only). FrameworkDetector handles it.
    ctx.add_step("parseConfig", "ok", ctx.step_ms());

    ctx.mode = ctx.assignment_config.mode;
    ctx.result["mode"] = ctx.mode;
    ctx.result["effectiveParams"]["mode"] = ctx.mode;
}

bool Pipeline::step_detect_framework(JobContext& ctx) const {
    ctx.emit_phase("detectFramework");
    auto detection = FrameworkDetector::detect(
        ctx.paths.solution_dir, ctx.assignment_config.allowed_frameworks);

    if(!detection.ok) {
        return fail_step(ctx, "detectFramework", detection.error_message, {
            {"allowedFrameworks", ctx.assignment_config.allowed_frameworks}
        });
    }
    ctx.add_step("detectFramework", "ok", ctx.step_ms());
    ctx.framework = detection.framework;

    ctx.result["assignmentConfig"] = {
        {"allowedFrameworks", ctx.assignment_config.allowed_frameworks},
        {"allowedPackages",   ctx.assignment_config.allowed_packages},
        {"framework",         ctx.framework},
        {"correctnessMode",   ctx.assignment_config.correctness_mode},
        {"mode",              ctx.mode}
    };
    if(!ctx.assignment_config.name.empty())
        ctx.result["assignmentConfig"]["name"] = ctx.assignment_config.name;
    LOG("Pipeline") << ctx.job_id << " | mode=" << ctx.mode
        << " | framework=" << ctx.framework << " (detected)\n";
    return true;
}

bool Pipeline::step_validate(JobContext& ctx) const {
    ctx.emit_phase("validation");
    fs::path cmake_path = fs::path(ctx.paths.solution_dir) / "CMakeLists.txt";
    bool has_student_cmake = fs::exists(cmake_path);
    ctx.result["effectiveParams"]["hasCMakeLists"] = has_student_cmake;

    if(has_student_cmake) {
        auto validation = CMakeValidator::validate(cmake_path, ctx.assignment_config.allowed_packages);
        if(!validation.valid) {
            std::string violations;
            for(const auto& v : validation.violations) {
                if(!violations.empty()) violations += ", ";
                violations += v;
            }
            return fail_step(ctx, "validation", "Forbidden dependencies: " + violations, {
                {"violations", validation.violations},
                {"allowedPackages", ctx.assignment_config.allowed_packages}
            });
        }
    }

    auto [fw_ok, fw_err] = build_service_.validate_framework(ctx.framework);
    if(!fw_ok) {
        return fail_step(ctx, "validation",
            "Framework '" + ctx.framework + "' unavailable: " + fw_err);
    }
    ctx.add_step("validation", "ok", ctx.step_ms());
    return true;
}

bool Pipeline::step_build_runner(JobContext& ctx,
                               std::function<void(job_status)>& status_updater) const {
    ctx.emit_phase("buildRunner", "Compiling student solution + runner");
    LOG("Pipeline") << ctx.job_id << " | building runner...\n";
    status_updater(job_status::building);

    // Drive the shadow_omp policy off allowedPackages: if the teacher allows
    // OpenMP as a direct package, raw <omp.h> is permitted; otherwise the
    // student must go through parallel_lib's <par/pragma.h>.
    bool shadow_omp = true;
    for(const auto& pkg : ctx.assignment_config.allowed_packages) {
        std::string lower;
        lower.reserve(pkg.size());
        for(char c : pkg) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if(lower == "openmp") { shadow_omp = false; break; }
    }

    ctx.runner_result = build_service_.build_runner(
        ctx.framework, ctx.paths.solution_dir, ctx.paths.test_dir, ctx.job_id, shadow_omp);
    ctx.runner_build_dir = ctx.runner_result.build_dir;

    if(!ctx.runner_result.success) {
        LOG_ERR("Pipeline") << ctx.job_id << " | Runner build failed:\n"
            << ctx.runner_result.build_output << "\n";
        ctx.result["buildOutput"] = sanitize(ctx.runner_result.build_output);
        return fail_step(ctx, "buildRunner",
            sanitize("Runner build failed: " + ctx.runner_result.error_message), {
                {"framework", ctx.framework},
                {"hasCMakeLists", ctx.result["effectiveParams"].value("hasCMakeLists", false)}
            });
    }
    ctx.add_step("buildRunner", "ok", ctx.step_ms());
    return true;
}

bool Pipeline::step_build_plugins(JobContext& ctx) const {
    ctx.emit_phase("buildPlugins", "Compiling teacher test plugins");
    LOG("Pipeline") << ctx.job_id << " | building test plugins...\n";

    ctx.plugin_result = build_service_.build_test_plugins(ctx.paths.test_dir);
    ctx.plugin_build_dir = ctx.plugin_result.build_dir;

    if(!ctx.plugin_result.success) {
        LOG_ERR("Pipeline") << ctx.job_id << " | Test plugin build failed:\n"
            << ctx.plugin_result.build_output << "\n";
        ctx.result["buildOutput"] = sanitize(ctx.plugin_result.build_output);
        return fail_step(ctx, "buildPlugins",
            sanitize("Test plugin build failed: " + ctx.plugin_result.error_message));
    }
    ctx.add_step("buildPlugins", "ok", ctx.step_ms());
    return true;
}

void Pipeline::step_load_plugins(JobContext& ctx,
                                TestRegistry& registry, PluginLoader& loader) const {
    ctx.emit_phase("loadPlugins");
    LOG("Pipeline") << ctx.job_id << " | loading test plugins...\n";

    auto plugins_loaded = nlohmann::json::array();
    for(const auto& plugin_path : ctx.plugin_result.plugin_paths) {
        std::string plugin_name = fs::path(plugin_path).filename().string();
        if(loader.load_plugin(plugin_path)) {
            plugins_loaded.push_back({{"name", plugin_name}, {"status", "loaded"}});
        } else {
            std::string err = "Failed to load plugin: " + plugin_name;
            LOG_ERR("Pipeline") << err << "\n";
            if(!ctx.plugin_load_error.empty()) ctx.plugin_load_error += "; ";
            ctx.plugin_load_error += err;
            plugins_loaded.push_back({{"name", plugin_name}, {"status", "failed"}});
        }
    }

    JsonScenarioLoader::load(ctx.paths.test_dir, registry);
    ctx.add_step("loadPlugins", ctx.plugin_load_error.empty() ? "ok" : "partial", ctx.step_ms());

    int corr_scenarios = 0, perf_scenarios = 0, corr_tests = 0, perf_tests = 0;
    for(const auto& scenario : registry.all()) {
        auto tests = scenario->get_tests();
        if(scenario->scenario_type() == ScenarioType::CORRECTNESS) {
            ++corr_scenarios; corr_tests += static_cast<int>(tests.size());
        } else {
            ++perf_scenarios; perf_tests += static_cast<int>(tests.size());
        }
    }
    ctx.result["testsDiscovered"] = {
        {"correctnessScenarios", corr_scenarios},
        {"correctnessTests", corr_tests},
        {"performanceScenarios", perf_scenarios},
        {"performanceTests", perf_tests},
        {"pluginsLoaded", plugins_loaded}
    };
}

Pipeline::LaneResults Pipeline::run_lane(JobContext& ctx, TestRegistry& registry,
                                         ScenarioType type, const std::string& step_name,
                                         bool is_perf) const {
    // step_name is also the camelCase phase name in pipeline[].step ("runCorrectness" / "runPerformance")
    ctx.emit_phase(step_name);

    // Sandbox-exclusivity for performance phase: drain in-flight correctness
    // phases of other jobs, block new ones, run alone.
    if(is_perf) lane_controller_.enter_perf_phase();
    else        lane_controller_.enter_correctness_phase();
    ScopeCleanup phase_guard{[&]() {
        if(is_perf) lane_controller_.exit_perf_phase();
        else        lane_controller_.exit_correctness_phase();
    }};

    LaneResults out;
    std::string monitor_mode = is_perf ? "normal" : ctx.assignment_config.correctness_mode;
    out.thread_counts = ThreadCounts::get(
        is_perf ? to_string(test_mode::performance) : to_string(test_mode::correctness),
        ctx.threads);
    long long memory_limit_kb = ctx.memory_limit_mb * 1024;
    auto extra_lib_dirs = build_service_.get_extra_lib_dirs(ctx.framework);
    out.results = test_executor_.run(
        ctx.runner_result.runner_exe_path, registry, type,
        out.thread_counts, monitor_mode, memory_limit_kb, ctx.job_id,
        ctx.wall_time_sec, ctx.cpu_time_sec,
        ctx.on_progress, ctx.node_id, extra_lib_dirs, ctx.max_processes);
    ctx.add_step(step_name, "ok", ctx.step_ms());
    return out;
}

bool Pipeline::all_tests_passed(const std::vector<TestScenarioResult>& results) {
    for(const auto& sr : results)
        for(const auto& tr : sr.results)
            if(!tr.passed) return false;
    return true;
}

void Pipeline::emit_lane_result(JobContext& ctx, const std::string& key, bool is_perf,
                              const LaneResults& lane, nlohmann::json& summary) {
    ctx.result[key] = TestScenarioResultConverter::to_grouped_json(
        lane.results, lane.thread_counts, is_perf);
    if(!ctx.result.contains("threadCounts") || !ctx.result["threadCounts"].is_object())
        ctx.result["threadCounts"] = nlohmann::json::object();
    ctx.result["threadCounts"][key] = lane.thread_counts;
    summary[key] = SandboxTestExecutor::build_summary(
        lane.results, lane.thread_counts, key);
}

void Pipeline::step_run_tests(JobContext& ctx, TestRegistry& registry,
                             std::function<void(job_status)>& status_updater) const {
    status_updater(job_status::running);
    LOG("Pipeline") << ctx.job_id << " | executing tests in sandbox...\n";

    nlohmann::json summary;

    try {
        if(ctx.mode == to_string(test_mode::all)) {
            auto corr = run_lane(ctx, registry, ScenarioType::CORRECTNESS, "runCorrectness", false);
            emit_lane_result(ctx, "correctness", false, corr, summary);

            if(all_tests_passed(corr.results) && ctx.plugin_load_error.empty()) {
                auto perf = run_lane(ctx, registry, ScenarioType::PERFORMANCE, "runPerformance", true);
                emit_lane_result(ctx, "performance", true, perf, summary);
                ctx.result["status"] = to_string(job_status::completed);
            } else {
                ctx.result["performanceSkipped"]    = true;
                ctx.result["failedStep"]            = "correctnessTests";
                ctx.result["performanceSkipReason"] = !ctx.plugin_load_error.empty()
                    ? "plugin load error" : "correctness tests failed";
                ctx.result["status"]                = to_string(job_status::failed);
            }
        } else {
            bool is_perf = (ctx.mode == to_string(test_mode::performance));
            ScenarioType type = is_perf ? ScenarioType::PERFORMANCE : ScenarioType::CORRECTNESS;
            const char* step_name = is_perf ? "runPerformance" : "runCorrectness";
            auto lane = run_lane(ctx, registry, type, step_name, is_perf);
            emit_lane_result(ctx, ctx.mode, is_perf, lane, summary);
            ctx.result["status"] = to_string(job_status::completed);
        }
    } catch(const std::exception& e) {
        fail_step(ctx, "executeTests",
            sanitize(std::string("Test execution failed: ") + e.what()));
    }

    if(!summary.is_null()) ctx.result["summary"] = summary;
}

// ============================================================================
// Public entry point
// ============================================================================

nlohmann::json Pipeline::execute(
    const nlohmann::json& request,
    std::function<void(job_status)> status_updater,
    progress::callback on_progress
) {
    JobContext ctx{request};
    ctx.on_progress = std::move(on_progress);
    init_job_context(ctx);

    // Finalisation MUST run before the result is copied out - i.e. cannot live
    // in a ScopeCleanup (those fire after the return-value construction). Use
    // a local lambda invoked at every exit point.
    auto finalize = [&]() -> nlohmann::json {
        if(!ctx.result.contains("totalTimeMs")) {
            double total_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - ctx.job_start).count();
            ctx.result["totalTimeMs"] = total_ms;
        }
        ctx.result["pipeline"] = ctx.pipeline;
        return ctx.result;
    };

    // Temp build-dir cleanup is fine in ScopeCleanup - runs after return, no JSON impact.
    ScopeCleanup build_cleanup{[&]() {
        if(!ctx.runner_build_dir.empty()) build_service_.cleanup(ctx.runner_build_dir);
        if(!ctx.plugin_build_dir.empty()) build_service_.cleanup(ctx.plugin_build_dir);
    }};

    if(!step_resolve(ctx))                       return finalize();
    step_parse_config(ctx);
    if(!step_detect_framework(ctx))               return finalize();
    if(!step_validate(ctx))                      return finalize();
    if(!step_build_runner(ctx, status_updater))   return finalize();
    if(!step_build_plugins(ctx))                  return finalize();

    TestRegistry registry;
    TestRegistry::set_active_instance(&registry);
    PluginLoader loader;
    // Clear registry BEFORE unloading DLLs - scenarios hold vtables in DLL memory.
    ScopeCleanup plugin_cleanup{[&]() {
        registry.clear();
        TestRegistry::clear_active_instance();
        loader.unload_all();
    }};

    step_load_plugins(ctx, registry, loader);

    step_run_tests(ctx, registry, status_updater);

    auto build_info = format_build_info(ctx.plugin_load_error);
    if(!build_info.empty()) ctx.result["buildInfo"] = build_info;

    LOG("Pipeline") << ctx.job_id << " | Done. Status: "
        << ctx.result.value("status", "unknown") << "\n";
    return finalize();
}
