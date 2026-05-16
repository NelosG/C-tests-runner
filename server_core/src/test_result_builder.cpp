// test_result_builder.cpp - split out of sandbox_test_executor.cpp.
// Owns the two pure post-processing helpers:
//   - build_test_result(): turn a single sandbox RunResult + verify() outcome
//                          into a populated TestResult
//   - build_summary():     aggregate a vector of TestScenarioResult into the
//                          JSON summary object (totals, failure breakdown,
//                          scalability across thread counts)
// Both are SandboxTestExecutor static methods, kept here to let the main
// sandbox_test_executor.cpp focus on the per-test sandbox-loop orchestration.

#include "sandbox_test_executor.h"

#include <path_sanitizer.h>
#include <test_data.h>

namespace fs = std::filesystem;

// ============================================================================
// buildTestResult - translate sandbox outcome + verify() into TestResult
// ============================================================================

TestResult SandboxTestExecutor::build_test_result(
    const Test& test,
    const SandboxLauncher::RunResult& run_result,
    const std::optional<nlohmann::json>& output_json,
    const TestData& input,
    const fs::path& output_dir
) {
    TestResult tr(test.name, false, "", 0.0);
    tr.exit_code = run_result.exit_code;
    tr.cg_mem_peak_kb = run_result.cg_mem_peak_kb;
    tr.max_rss_kb = run_result.max_rss_kb;
    tr.cpu_time_sec = run_result.cpu_time_sec;
    tr.wall_time_sec = run_result.wall_time_sec;
    tr.oom_killed = run_result.oom_killed;
    tr.timed_out = run_result.timed_out;
    tr.stderr_output = path_sanitizer::sanitize(run_result.stderr_output);

    if(run_result.timed_out) {
        tr.message = "Time limit exceeded";
        return tr;
    }
    if(run_result.oom_killed) {
        tr.message = "Out of memory";
        return tr;
    }
    if(run_result.exit_code != 0) {
        tr.message = (run_result.exit_code < 0)
            ? "Process crashed (signal " + std::to_string(-run_result.exit_code) + ")"
            : "Process exited with code " + std::to_string(run_result.exit_code);
        return tr;
    }

    // Runner exited cleanly. meta.bin may still be missing (e.g. crash inside
    // runner::finish after output.bin was written) - that's a degraded but
    // recoverable state: stats stay at zero, but we still call verify() on
    // whatever output.bin holds. Only treat it as "no output" if BOTH meta and
    // output.bin are missing.
    if(!output_json.has_value()) {
        if(!fs::exists(output_dir / "output.bin")) {
            tr.message = "Runner produced no output";
            return tr;
        }
    }

    const nlohmann::json& j = output_json.has_value()
        ? output_json.value()
        : nlohmann::json::object();
    tr.time_ms = j.value("timeMs", 0.0);

    if(j.contains("parallelStats")) {
        const auto& ps = j["parallelStats"];
        tr.parallel_regions = ps.value("parallelRegions", 0);
        tr.tasks_created = ps.value("tasksCreated", 0);
        tr.max_threads_used = ps.value("maxThreadsUsed", 0);
        tr.single_regions = ps.value("singleRegions", 0);
        tr.taskwaits = ps.value("taskWaits", 0);
        tr.barriers = ps.value("barriers", 0);
        tr.criticals = ps.value("criticals", 0);
        tr.for_loops = ps.value("forLoops", 0);
        tr.atomics = ps.value("atomics", 0);
        tr.sections = ps.value("sections", 0);
        tr.masters = ps.value("masters", 0);
        tr.ordered = ps.value("ordered", 0);
        tr.taskgroups = ps.value("taskGroups", 0);
        tr.simd_constructs = ps.value("simdConstructs", 0);
        tr.cancels = ps.value("cancels", 0);
        tr.flushes = ps.value("flushes", 0);
        tr.taskyields = ps.value("taskYields", 0);
        tr.work_ns = ps.value("workNs", static_cast<long long>(0));
        tr.span_ns = ps.value("spanNs", static_cast<long long>(0));
    }

    // Wall-clock span fallback: only when monitoring was off (NORMAL mode).
    if(tr.work_ns == 0 && tr.span_ns == 0) {
        tr.span_ns = static_cast<long long>(tr.time_ms * 1e6);
    }

    try {
        TestData output = TestData::load(output_dir / "output.bin");
        auto [passed, message] = test.verify(input, output);
        tr.passed = passed;
        tr.message = message;
    } catch(const std::exception& e) {
        tr.passed = false;
        tr.message = std::string("Verify threw exception: ") + e.what();
    }

    return tr;
}

// ============================================================================
// buildSummary - aggregate stats across results
// ============================================================================

nlohmann::json SandboxTestExecutor::build_summary(
    const std::vector<TestScenarioResult>& results,
    const std::vector<int>& thread_counts,
    const std::string& /*label*/

) {
    nlohmann::json summary;
    int total = 0, passed = 0, failed = 0;
    int failed_timeout = 0, failed_oom = 0, failed_crash = 0, failed_correctness = 0;
    double max_time_ms = 0.0;
    long long max_rss_kb = 0, max_cg_mem_kb = 0;
    double total_cpu_sec = 0.0;

    for(const auto& sr : results) {
        for(const auto& tr : sr.results) {
            ++total;
            if(tr.time_ms > max_time_ms) max_time_ms = tr.time_ms;
            if(tr.max_rss_kb > max_rss_kb) max_rss_kb = tr.max_rss_kb;
            if(tr.cg_mem_peak_kb > max_cg_mem_kb) max_cg_mem_kb = tr.cg_mem_peak_kb;
            total_cpu_sec += tr.cpu_time_sec;

            if(tr.passed) {
                ++passed;
                continue;
            }
            ++failed;
            if(tr.timed_out) ++failed_timeout;
            else if(tr.oom_killed) ++failed_oom;
            else if(tr.exit_code != 0) ++failed_crash;
            else ++failed_correctness;
        }
    }

    summary["totalTests"] = total;
    summary["passed"] = passed;
    summary["failed"] = failed;
    summary["failedByTimeout"] = failed_timeout;
    summary["failedByOom"] = failed_oom;
    summary["failedByCrash"] = failed_crash;
    summary["failedByCorrectness"] = failed_correctness;
    summary["maxTimeMs"] = max_time_ms;
    summary["maxRssKb"] = max_rss_kb;
    summary["maxCgMemPeakKb"] = max_cg_mem_kb;
    summary["totalCpuTimeSec"] = total_cpu_sec;

    // Scalability: speedup at each thread count vs t=1 baseline.
    if(thread_counts.size() < 2 || results.empty()) return summary;

    size_t num_scenarios = results.size() / thread_counts.size();
    if(num_scenarios == 0 || results.size() != num_scenarios * thread_counts.size())
        return summary;

    auto scalability = nlohmann::json::array();
    for(size_t t = 0; t < thread_counts.size(); ++t) {
        double t_total = 0.0, t1_total = 0.0, t_cpu_sec = 0.0;
        long long t_max_rss = 0;
        for(size_t s = 0; s < num_scenarios; ++s) {
            const auto& sr_t = results[t * num_scenarios + s];
            const auto& sr_1 = results[s];
            for(size_t i = 0; i < sr_t.results.size() && i < sr_1.results.size(); ++i) {
                t_total += sr_t.results[i].time_ms;
                t1_total += sr_1.results[i].time_ms;
                t_cpu_sec += sr_t.results[i].cpu_time_sec;
                if(sr_t.results[i].max_rss_kb > t_max_rss)
                    t_max_rss = sr_t.results[i].max_rss_kb;
            }
        }
        double speedup = (t_total > 0.0) ? t1_total / t_total : 0.0;
        int tc = thread_counts[t];
        scalability.push_back(
            {
                {"threads", tc},
                {"totalTimeMs", t_total},
                {"speedup", speedup},
                {"efficiency", (tc > 0) ? speedup / tc : 0.0},
                {"maxRssKb", t_max_rss},
                {"totalCpuTimeSec", t_cpu_sec}
            }
        );
    }
    summary["scalability"] = scalability;
    return summary;
}
