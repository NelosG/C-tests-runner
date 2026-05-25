#pragma once

/**
 * @file pipeline.h
 * @brief Single-job execution pipeline: resolve -> build -> run -> report.
 *
 * The pipeline transforms a TaskSubmission JSON into a TaskResult JSON.
 * Each step is a private method that mutates a JobContext (per-call state)
 * and returns true to continue or false to halt with a populated error.
 *
 * Pipeline is stateless across jobs - multiple JobQueue workers may invoke
 * execute() concurrently as long as the injected services are themselves
 * thread-safe (BuildService writes to per-job temp dirs; SandboxTestExecutor
 * uses the per-job runner exe).
 */

#include <api_types.h>
#include <assignment_config.h>
#include <build_service.h>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <plugin_loader.h>
#include <progress_callback.h>
#include <resource_manager.h>
#include <sandbox_test_executor.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class TestRegistry;

class Pipeline {
    public:
        /// Interface a Pipeline uses to enforce sandbox-exclusive performance phases.
        /// JobQueue implements it; tests can stub it out with no-ops.
        struct LaneController {
            virtual ~LaneController() = default;
            virtual void enter_correctness_phase() = 0;
            virtual void exit_correctness_phase() = 0;
            virtual void enter_perf_phase() = 0;
            virtual void exit_perf_phase() = 0;
        };

        Pipeline(
            BuildService& build_service,
            SandboxTestExecutor& test_executor,
            ResourceManager& resource_manager,
            LaneController& lane_controller
        );

        /// Run one job through the full pipeline. Result is the TaskResult JSON.
        nlohmann::json execute(
            const nlohmann::json& request,
            const std::string& node_id,
            std::function<void(job_status)> status_updater,
            progress::callback on_progress = {}
        );

        struct ResolvedPaths {
            std::string test_dir;
            std::string solution_dir;
        };

        /// Per-execute() state threaded through pipeline steps. Public for
        /// unit-test access; production callers are in pipeline_steps.cpp.
        struct JobContext {
            // Inputs (from request)
            const nlohmann::json& request;
            std::string job_id;
            std::string test_id;
            std::string node_id;
            int threads = 4;
            long long memory_limit_mb = 0;
            int wall_time_sec = 60;
            int cpu_time_sec = 30;
            int max_processes = 0;     ///< 0 -> derive from threads * sandbox_process_multiplier
            std::string solution_name;
            progress::callback on_progress;

            // Accumulating output
            nlohmann::json result;
            nlohmann::json pipeline = nlohmann::json::array();

            // Step outputs
            ResolvedPaths paths;
            AssignmentConfig assignment_config;
            std::string framework;     // detected
            std::string mode;
            BuildService::RunnerBuildResult runner_result;
            BuildService::TestPluginBuildResult plugin_result;
            std::string runner_build_dir;
            std::string plugin_build_dir;
            std::string plugin_load_error;

            // Timing
            std::chrono::steady_clock::time_point job_start = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point step_start = job_start;

            void add_step(const std::string& name, const std::string& status, double ms);
            double step_ms();
            void emit_phase(
                const std::string& phase,
                const std::string& message = "",
                std::optional<double> progress = std::nullopt
            ) const;
        };

        /// Run a single sandbox lane (correctness OR performance). Public so
        /// unit tests can assemble LaneResults directly.
        struct LaneResults {
            std::vector<TestScenarioResult> results;
            std::vector<int> thread_counts;
        };

        /// Mark a pipeline step as failed and populate ctx.result with diagnostics.
        /// Always returns false so callers can `return fail_step(...);`. Public
        /// for unit-test access; production callers are pipeline_steps.cpp.
        static bool fail_step(
            JobContext& ctx,
            const std::string& step,
            const std::string& error_message,
            nlohmann::json extra_details = {}
        );

        /// Returns true iff every test in `results` is .passed.
        static bool all_tests_passed(const std::vector<TestScenarioResult>& results);

        /// Emit a finished lane's grouped results, threadCounts entry, and summary
        /// section under the given key (e.g. "correctness" / "performance").
        static void emit_lane_result(
            JobContext& ctx,
            const std::string& key,
            bool is_perf,
            const LaneResults& lane,
            nlohmann::json& summary
        );

    private:
        void init_job_context(JobContext& ctx) const;
        ResolvedPaths resolve_paths(const nlohmann::json& request, const JobContext& ctx) const;

        // Pipeline steps. Steps that may fail return bool.
        bool step_resolve(JobContext& ctx) const;
        void step_parse_config(JobContext& ctx) const;
        bool step_detect_framework(JobContext& ctx) const;
        bool step_validate(JobContext& ctx) const;
        bool step_build_runner(JobContext& ctx, std::function<void(job_status)>& status_updater) const;
        bool step_build_plugins(JobContext& ctx) const;
        void step_load_plugins(JobContext& ctx, TestRegistry& registry, PluginLoader& loader) const;
        void step_run_tests(
            JobContext& ctx,
            TestRegistry& registry,
            std::function<void(job_status)>& status_updater
        ) const;

        LaneResults run_lane(
            JobContext& ctx,
            TestRegistry& registry,
            ScenarioType type,
            const std::string& step_name,
            bool is_perf
        ) const;

        BuildService& build_service_;
        SandboxTestExecutor& test_executor_;
        ResourceManager& resource_manager_;
        LaneController& lane_controller_;
};
