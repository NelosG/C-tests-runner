#pragma once

/**
 * @file sandbox_test_executor.h
 * @brief Runs registered tests in sandboxed processes and aggregates results.
 *
 * Pipeline per test:
 *   1. test.setup(input_dir)            - once per test, cached across thread counts
 *   2. sandbox.execute(runner_exe, ...) - student code runs isolated
 *   3. test.verify(input_dir, output_dir) -> TestResult
 *
 * Test plugin code (setup/verify) runs in the parent process; only the runner
 * exe (which contains student code) runs inside the sandbox.
 */

#include <cpu_isolator.h>
#include <filesystem>
#include <optional>
#include <progress_callback.h>
#include <sandbox_launcher.h>
#include <string>
#include <test.h>
#include <test_result.h>
#include <test_scenario_extension.h>
#include <test_scenario_result.h>
#include <vector>
#include <nlohmann/json.hpp>

class TestRegistry;

class SandboxTestExecutor {
    public:
        SandboxTestExecutor(SandboxLauncher& sandbox, CpuIsolator& cpu_isolator);

        /// Run every registered test of the given scenario type, once per thread count.
        /// Result ordering is [scenario0@tc0, scenario1@tc0, ..., scenario0@tc1, ...]
        /// to match TestScenarioResultConverter::to_grouped_json expectations.
        std::vector<TestScenarioResult> run(
            const std::string& runner_exe,
            const TestRegistry& registry,
            ScenarioType type_filter,
            const std::vector<int>& thread_counts,
            const std::string& monitor_mode,
            long long memory_limit_kb,
            const std::string& job_id,
            int wall_time_sec,
            int cpu_time_sec,
            progress::callback on_progress = {},
            const std::string& node_id = "",
            const std::vector<std::string>& extra_lib_dirs = {},
            int max_processes = 0,  ///< 0 = use thread_count * 2 fallback
            int warmup_iterations = 0  ///< 0 = no warmup
        );

        /// Compute totals + scalability summary across ALL scenarios.
        /// Scalability uses pairwise gating (test contributes to ladder
        /// entry T only if it passed at both T=1 baseline AND T) and adds
        /// `testsCompared` / `testsSkipped` per entry. Entries with zero
        /// compared tests are omitted; if no entry is valid (baseline broken),
        /// `scalability` is omitted entirely.
        static nlohmann::json build_summary(
            const std::vector<TestScenarioResult>& results,
            const std::vector<int>& thread_counts,
            const std::string& label
        );

        /// Same aggregation as build_summary but scoped to one scenario
        /// (selected by index in 0..num_scenarios). Used by the converter
        /// to attach a per-scenario summary block.
        static nlohmann::json build_scenario_summary(
            const std::vector<TestScenarioResult>& results,
            const std::vector<int>& thread_counts,
            size_t scenario_index
        );

        /// Convert a sandbox RunResult + verify() outcome into a TestResult.
        /// Public for unit-test access; no external callers in production.
        static TestResult build_test_result(
            const Test& test,
            const SandboxLauncher::RunResult& run_result,
            const std::optional<nlohmann::json>& output_json,
            const TestData& input,
            const std::filesystem::path& output_dir
        );

    private:
        SandboxLauncher& sandbox_;
        CpuIsolator& cpu_isolator_;
};
