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

#include <fstream>
#include <iterator>
#include <path_sanitizer.h>
#include <test_data.h>
#include <vector>

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
        if(test.has_expected_output()) {
            // Byte-compare output.bin with the supplied expected blob.
            // Used together with raw_input_path for "feed pbbs's bench
            // bytes through our engine" comparisons.
            std::ifstream got(output_dir / "output.bin", std::ios::binary);
            std::ifstream exp(test.expected_output_path, std::ios::binary);
            if(!got || !exp) {
                tr.passed = false;
                tr.message = "Failed to open output / expected file";
            } else {
                std::vector<char> bg((std::istreambuf_iterator<char>(got)),
                                     std::istreambuf_iterator<char>());
                std::vector<char> be((std::istreambuf_iterator<char>(exp)),
                                     std::istreambuf_iterator<char>());
                if(bg.size() != be.size()) {
                    tr.passed = false;
                    tr.message = "output size " + std::to_string(bg.size())
                        + " != expected " + std::to_string(be.size());
                } else if(bg != be) {
                    tr.passed = false;
                    tr.message = "output bytes differ from expected";
                } else {
                    tr.passed = true;
                }
            }
        } else {
            TestData output = TestData::load(output_dir / "output.bin");
            auto [passed, message] = test.verify(input, output);
            tr.passed = passed;
            tr.message = message;
        }
    } catch(const std::exception& e) {
        tr.passed = false;
        tr.message = std::string("Verify threw exception: ") + e.what();
    }

    return tr;
}

// ============================================================================
// Shared aggregation helpers
// ============================================================================
//
// The `results` vector has a strict layout:
//   [T1: scen0, scen1, ..., scenN] [T2: scen0, scen1, ..., scenN] ...
// `scenario_indices` selects which scenarios (by index in 0..num_scenarios)
// contribute to the aggregate. With one index it produces per-scenario stats;
// with all indices it produces the job-wide summary.
//
// Scalability uses pairwise gating: a test contributes to the ladder entry for
// thread count T only if it passed at BOTH the T=1 baseline AND T. This keeps
// failed tests (timeouts, crashes, OOM kills) from poisoning the speedup -
// crashes typically return time_ms near zero (fake "1000x speedup") and
// timeouts return time_ms ~= wall_limit (fake "0.001x speedup"). Entries with
// zero compared tests are omitted entirely, and if the baseline itself has no
// passing tests the entire scalability array is omitted.

namespace {

    nlohmann::json build_scalability(
        const std::vector<TestScenarioResult>& results,
        const std::vector<int>& thread_counts,
        size_t num_scenarios,
        const std::vector<size_t>& scenario_indices
    ) {
        if(thread_counts.size() < 2 || results.empty() || scenario_indices.empty())
            return nlohmann::json::array();

        auto scalability = nlohmann::json::array();

        for(size_t t = 0; t < thread_counts.size(); ++t) {
            double total_time_ms = 0.0;
            double baseline_time_ms = 0.0;
            double total_cpu_sec = 0.0;
            long long max_rss_kb = 0;
            int tests_compared = 0;
            int tests_skipped = 0;

            for(size_t s : scenario_indices) {
                const auto& sr_t = results[t * num_scenarios + s];
                const auto& sr_baseline = results[s];   // T=1, first slab
                size_t n = std::min(sr_t.results.size(),
                                    sr_baseline.results.size());
                for(size_t i = 0; i < n; ++i) {
                    const auto& r_t = sr_t.results[i];
                    const auto& r_baseline = sr_baseline.results[i];
                    if(!r_t.passed || !r_baseline.passed) {
                        ++tests_skipped;
                        continue;
                    }
                    ++tests_compared;
                    total_time_ms += r_t.time_ms;
                    baseline_time_ms += r_baseline.time_ms;
                    total_cpu_sec += r_t.cpu_time_sec;
                    if(r_t.max_rss_kb > max_rss_kb)
                        max_rss_kb = r_t.max_rss_kb;
                }
            }

            // Drop entries that have no comparable pair at this thread count;
            // reporting a number for an empty set would be a lie.
            if(tests_compared == 0) continue;

            double speedup = (total_time_ms > 0.0)
                ? baseline_time_ms / total_time_ms
                : 0.0;
            int tc = thread_counts[t];
            scalability.push_back({
                {"threads", tc},
                {"totalTimeMs", total_time_ms},
                {"speedup", speedup},
                {"efficiency", (tc > 0) ? speedup / tc : 0.0},
                {"maxRssKb", max_rss_kb},
                {"totalCpuTimeSec", total_cpu_sec},
                {"testsCompared", tests_compared},
                {"testsSkipped", tests_skipped}
            });
        }

        return scalability;
    }

    nlohmann::json build_aggregate(
        const std::vector<TestScenarioResult>& results,
        const std::vector<int>& thread_counts,
        size_t num_scenarios,
        const std::vector<size_t>& scenario_indices
    ) {
        nlohmann::json summary;
        int total = 0, passed = 0, failed = 0;
        int failed_timeout = 0, failed_oom = 0;
        int failed_crash = 0, failed_correctness = 0;
        double max_time_ms = 0.0;
        long long max_rss_kb = 0, max_cg_mem_kb = 0;
        double total_cpu_sec = 0.0;

        for(size_t t = 0; t < thread_counts.size(); ++t) {
            for(size_t s : scenario_indices) {
                if(t * num_scenarios + s >= results.size()) continue;
                const auto& sr = results[t * num_scenarios + s];
                for(const auto& tr : sr.results) {
                    ++total;
                    if(tr.time_ms > max_time_ms) max_time_ms = tr.time_ms;
                    if(tr.max_rss_kb > max_rss_kb) max_rss_kb = tr.max_rss_kb;
                    if(tr.cg_mem_peak_kb > max_cg_mem_kb)
                        max_cg_mem_kb = tr.cg_mem_peak_kb;
                    total_cpu_sec += tr.cpu_time_sec;

                    if(tr.passed) { ++passed; continue; }
                    ++failed;
                    if(tr.timed_out) ++failed_timeout;
                    else if(tr.oom_killed) ++failed_oom;
                    else if(tr.exit_code != 0) ++failed_crash;
                    else ++failed_correctness;
                }
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

        auto scal = build_scalability(results, thread_counts,
                                      num_scenarios, scenario_indices);
        if(!scal.empty()) summary["scalability"] = scal;
        return summary;
    }

} // namespace

// ============================================================================
// buildSummary - job-wide aggregate across all scenarios
// ============================================================================

nlohmann::json SandboxTestExecutor::build_summary(
    const std::vector<TestScenarioResult>& results,
    const std::vector<int>& thread_counts,
    const std::string& /*label*/
) {
    if(results.empty() || thread_counts.empty())
        return nlohmann::json::object();

    size_t num_scenarios = results.size() / thread_counts.size();
    if(num_scenarios == 0 || results.size() != num_scenarios * thread_counts.size())
        return nlohmann::json::object();

    std::vector<size_t> all_scenarios(num_scenarios);
    for(size_t s = 0; s < num_scenarios; ++s) all_scenarios[s] = s;
    return build_aggregate(results, thread_counts, num_scenarios, all_scenarios);
}

// ============================================================================
// buildScenarioSummary - per-scenario aggregate (used by the JSON converter
// to attach a summary block to each correctness[] / performance[] entry)
// ============================================================================

nlohmann::json SandboxTestExecutor::build_scenario_summary(
    const std::vector<TestScenarioResult>& results,
    const std::vector<int>& thread_counts,
    size_t scenario_index
) {
    if(results.empty() || thread_counts.empty())
        return nlohmann::json::object();

    size_t num_scenarios = results.size() / thread_counts.size();
    if(num_scenarios == 0 || results.size() != num_scenarios * thread_counts.size())
        return nlohmann::json::object();
    if(scenario_index >= num_scenarios)
        return nlohmann::json::object();

    return build_aggregate(results, thread_counts, num_scenarios,
                           std::vector<size_t>{scenario_index});
}
