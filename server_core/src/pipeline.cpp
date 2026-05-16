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
    const std::string& name,
    const std::string& status,
    double ms
) {
    pipeline.push_back({{"step", name}, {"status", status}, {"durationMs", ms}});
}

double Pipeline::JobContext::step_ms() {
    auto now = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - step_start).count();
    step_start = now;
    return ms;
}

void Pipeline::JobContext::emit_phase(
    const std::string& phase,
    const std::string& message,
    std::optional<double> progress
) const {
    if(!on_progress) return;
    nlohmann::json event = {
        {"jobId", job_id},
        {"nodeId", node_id},
        {"phase", phase},
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

bool Pipeline::fail_step(
    JobContext& ctx,
    const std::string& step,
    const std::string& error_message,
    nlohmann::json extra_details
) {
    ctx.add_step(step, "failed", ctx.step_ms());
    ctx.result["status"] = to_string(job_status::failed);
    ctx.result["failedStep"] = step;
    ctx.result["error"] = error_message;
    // Accept only object payloads here - assigning to operator[] on non-object
    // non-null JSON throws type_error. This is defensive against future callers.
    nlohmann::json details = extra_details.is_object()
        ? std::move(extra_details)
        : nlohmann::json::object();
    details["step"] = step;
    ctx.result["errorDetails"] = std::move(details);
    return false;
}

// ============================================================================
// Construction
// ============================================================================

Pipeline::Pipeline(
    BuildService& build_service,
    SandboxTestExecutor& test_executor,
    ResourceManager& resource_manager,
    LaneController& lane_controller
)
    : build_service_(build_service),
      test_executor_(test_executor),
      resource_manager_(resource_manager),
      lane_controller_(lane_controller) {}

void Pipeline::init_job_context(JobContext& ctx) const {
    ctx.job_id = ctx.request.at("jobId").get<std::string>();
    ctx.test_id = ctx.request.at("testId").get<std::string>();
    // node_id is supplied by Pipeline::execute()'s caller (the adapter
    // injects its config_.node_id; CLI passes ""). We don't pull it from
    // the request JSON - the request comes from the orchestrator and we
    // don't mutate it with engine-internal fields.
    // Read LIVE atomics - these may be mutated concurrently by apply_config().
    const long long mem_default = build_service_.default_memory_limit_mb();
    const int threads_default = build_service_.default_threads();
    const int wall_default = build_service_.default_wall_time_sec();
    const int cpu_default = build_service_.default_cpu_time_sec();
    const int proc_multiplier = build_service_.sandbox_process_multiplier();
    ctx.threads = ctx.request.value("threads", threads_default);
    ctx.memory_limit_mb = ctx.request.value("memoryLimitMb", mem_default);
    ctx.wall_time_sec = ctx.request.value("wallTimeSec", wall_default);
    ctx.cpu_time_sec = ctx.request.value("cpuTimeSec", cpu_default);
    ctx.max_processes = ctx.request.value(
        "maxProcesses",
        ctx.threads * proc_multiplier
    );
    ctx.solution_name = request_helpers::extract_solution_name(ctx.request);

    ctx.result["jobId"] = ctx.job_id;
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
        { "platform", "linux" },
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

    if(paths.test_dir.empty()) throw std::runtime_error("testSource resolved to empty path");
    if(paths.solution_dir.empty()) throw std::runtime_error("solutionSource resolved to empty path");
    return paths;
}

// ============================================================================
// Pipeline steps live in pipeline_steps.cpp - the eight step_* methods plus
// the lane helpers (run_lane / emit_lane_result / all_tests_passed). The
// execute() entry point below stitches them together.
// ============================================================================


// ============================================================================
// Public entry point
// ============================================================================

nlohmann::json Pipeline::execute(
    const nlohmann::json& request,
    const std::string& node_id,
    std::function<void(job_status)> status_updater,
    progress::callback on_progress
) {
    JobContext ctx{request};
    ctx.node_id = node_id;
    ctx.on_progress = std::move(on_progress);
    init_job_context(ctx);

    // Finalisation MUST run before the result is copied out - i.e. cannot live
    // in a ScopeCleanup (those fire after the return-value construction). Use
    // a local lambda invoked at every exit point.
    auto finalize = [&]() -> nlohmann::json {
        if(!ctx.result.contains("totalTimeMs")) {
            double total_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - ctx.job_start
            ).count();
            ctx.result["totalTimeMs"] = total_ms;
        }
        ctx.result["pipeline"] = ctx.pipeline;
        return ctx.result;
    };

    // Temp build-dir cleanup is fine in ScopeCleanup - runs after return, no JSON impact.
    ScopeCleanup build_cleanup{
        [&]() {
            if(!ctx.runner_build_dir.empty()) build_service_.cleanup(ctx.runner_build_dir);
            if(!ctx.plugin_build_dir.empty()) build_service_.cleanup(ctx.plugin_build_dir);
        }
    };

    if(!step_resolve(ctx)) return finalize();
    step_parse_config(ctx);
    if(!step_detect_framework(ctx)) return finalize();
    if(!step_validate(ctx)) return finalize();
    if(!step_build_runner(ctx, status_updater)) return finalize();
    if(!step_build_plugins(ctx)) return finalize();

    TestRegistry registry;
    TestRegistry::set_active_instance(&registry);
    PluginLoader loader;
    // Clear registry BEFORE unloading DLLs - scenarios hold vtables in DLL memory.
    ScopeCleanup plugin_cleanup{
        [&]() {
            registry.clear();
            TestRegistry::clear_active_instance();
            loader.unload_all();
        }
    };

    step_load_plugins(ctx, registry, loader);

    step_run_tests(ctx, registry, status_updater);

    auto build_info = format_build_info(ctx.plugin_load_error);
    if(!build_info.empty()) ctx.result["buildInfo"] = build_info;

    LOG("Pipeline") << ctx.job_id << " | Done. Status: "
        << ctx.result.value("status", "unknown") << "\n";
    return finalize();
}
