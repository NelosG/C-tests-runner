// Unit tests for SandboxTestExecutor::build_summary.
//
// build_summary is a pure aggregation over a vector of TestScenarioResult:
//   - totals (passed/failed)
//   - failure breakdown (timeout / oom / crash / correctness)
//   - resource peaks (maxRssKb, maxCgMemPeakKb, totalCpuTimeSec)
//   - scalability across thread counts (speedup, efficiency)
//
// We never spin up a real sandbox here - just build TestResult / TestScenarioResult
// values in memory and assert on the JSON shape & numbers.

#include <gtest/gtest.h>
#include <sandbox_test_executor.h>
#include <test_result.h>
#include <test_scenario_result.h>


namespace {

    /// Helper - produce a TestResult with the fields build_summary classifies on.
    TestResult mk(
        bool passed,
        double time_ms = 1.0,
        long long rss_kb = 0,
        double cpu_sec = 0.0,
        bool timed_out = false,
        bool oom = false,
        int exit_code = 0
    ) {
        TestResult r("t", passed, "", time_ms);
        r.max_rss_kb = rss_kb;
        r.cpu_time_sec = cpu_sec;
        r.timed_out = timed_out;
        r.oom_killed = oom;
        r.exit_code = exit_code;
        return r;
    }

} // namespace

// -----------------------------------------------------------------------------
// Empty / edge inputs
// -----------------------------------------------------------------------------

TEST(SandboxSummary, empty_results_returns_zeroed_totals_no_scalability) {
    auto j = SandboxTestExecutor::build_summary({}, {1, 2, 4}, "label");
    EXPECT_EQ(j["totalTests"], 0);
    EXPECT_EQ(j["passed"], 0);
    EXPECT_EQ(j["failed"], 0);
    EXPECT_FALSE(j.contains("scalability"))
        << "no results -> no scalability block (avoids divide-by-zero)";
}

TEST(SandboxSummary, single_thread_count_omits_scalability_block) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{mk(true)}, 1);
    auto j = SandboxTestExecutor::build_summary(results, {1}, "label");
    EXPECT_FALSE(j.contains("scalability"))
        << "speedup requires at least two thread counts";
}

// -----------------------------------------------------------------------------
// Failure classification - each failure mode maps to one bucket
// -----------------------------------------------------------------------------

TEST(SandboxSummary, classifies_each_failure_into_exactly_one_bucket) {
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{
            mk(true),                                                  // pass
            mk(false, 1.0, 0, 0.0, true),                  // timeout
            mk(false, 1.0, 0, 0.0, false, true),                // oom
            mk(false, 1.0, 0, 0.0, false, false, 137),    // crash
            mk(false, 1.0, 0, 0.0, false, false, 0)                     // exit=0 + !passed -> correctness
        },
        1
    );

    auto j = SandboxTestExecutor::build_summary(results, {1}, "label");
    EXPECT_EQ(j["totalTests"], 5);
    EXPECT_EQ(j["passed"], 1);
    EXPECT_EQ(j["failed"], 4);
    EXPECT_EQ(j["failedByTimeout"], 1);
    EXPECT_EQ(j["failedByOom"], 1);
    EXPECT_EQ(j["failedByCrash"], 1);
    EXPECT_EQ(j["failedByCorrectness"], 1);
}

TEST(SandboxSummary, timeout_takes_precedence_over_crash_classification) {
    // If a result is BOTH timed-out and has exit_code != 0, the timeout branch
    // wins (it's the first check). Documenting the priority ordering.
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{
            mk(false, 1.0, 0, 0.0, true, false, -9)
        },
        1
    );
    auto j = SandboxTestExecutor::build_summary(results, {1}, "label");
    EXPECT_EQ(j["failedByTimeout"], 1);
    EXPECT_EQ(j["failedByCrash"], 0);
}

// -----------------------------------------------------------------------------
// Resource peak tracking
// -----------------------------------------------------------------------------

TEST(SandboxSummary, tracks_maxima_and_sums_cpu_time_across_results) {
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{
            mk(true, 10.0, 1024, 0.5),
            mk(true, 25.0, 2048, 1.0)
        },
        1
    );
    results.back().results[1].cg_mem_peak_kb = 4096;  // peak via cgroup

    auto j = SandboxTestExecutor::build_summary(results, {1}, "label");
    EXPECT_DOUBLE_EQ(j["maxTimeMs"].get<double>(), 25.0);
    EXPECT_EQ(j["maxRssKb"].get<long long>(), 2048);
    EXPECT_EQ(j["maxCgMemPeakKb"].get<long long>(), 4096);
    EXPECT_DOUBLE_EQ(j["totalCpuTimeSec"].get<double>(), 1.5);
}

// -----------------------------------------------------------------------------
// Scalability block - speedup vs threads=1 baseline
// -----------------------------------------------------------------------------

TEST(SandboxSummary, scalability_speedup_and_efficiency) {
    // Layout: [s0@tc=1, s0@tc=4]. t1_total=100ms, tp_total=25ms -> speedup=4.
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{mk(true, 100.0)}, 1);
    results.emplace_back("S", std::vector<TestResult>{mk(true, 25.0)}, 4);

    auto j = SandboxTestExecutor::build_summary(results, {1, 4}, "perf");
    ASSERT_TRUE(j.contains("scalability"));
    const auto& sc = j["scalability"];
    ASSERT_EQ(sc.size(), 2u);

    EXPECT_EQ(sc[0]["threads"].get<int>(), 1);
    EXPECT_DOUBLE_EQ(sc[0]["speedup"].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(sc[0]["efficiency"].get<double>(), 1.0);

    EXPECT_EQ(sc[1]["threads"].get<int>(), 4);
    EXPECT_DOUBLE_EQ(sc[1]["speedup"].get<double>(), 4.0);
    EXPECT_DOUBLE_EQ(sc[1]["efficiency"].get<double>(), 1.0);
}

TEST(SandboxSummary, scalability_speedup_zero_when_total_time_zero) {
    // All-zero times must not divide-by-zero; speedup falls back to 0.0.
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{mk(true, 0.0)}, 1);
    results.emplace_back("S", std::vector<TestResult>{mk(true, 0.0)}, 4);
    auto j = SandboxTestExecutor::build_summary(results, {1, 4}, "perf");
    EXPECT_DOUBLE_EQ(j["scalability"][1]["speedup"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["scalability"][1]["efficiency"].get<double>(), 0.0);
}

TEST(SandboxSummary, scalability_omitted_when_size_layout_is_invalid) {
    // 3 scenarios with 2 thread counts -> 3 % 2 != 0 -> invalid layout -> skip.
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{mk(true, 1.0)}, 1);
    results.emplace_back("S", std::vector<TestResult>{mk(true, 1.0)}, 2);
    results.emplace_back("BAD", std::vector<TestResult>{mk(true, 1.0)}, 4);
    auto j = SandboxTestExecutor::build_summary(results, {1, 2}, "label");
    EXPECT_FALSE(j.contains("scalability"));
}

TEST(SandboxSummary, scalability_handles_multi_scenario_multi_test_aggregation) {
    // 2 scenarios x 2 thread counts x 2 tests each.
    // Layout: [s0@1, s1@1, s0@2, s1@2]
    auto pair_at = [](double t1, double t2) {
        return std::vector<TestResult>{mk(true, t1), mk(true, t2)};
    };
    std::vector<TestScenarioResult> results;
    results.emplace_back("Alpha", pair_at(50, 50), 1);   // s0@1: 100 total
    results.emplace_back("Beta", pair_at(40, 60), 1);   // s1@1: 100 total
    results.emplace_back("Alpha", pair_at(25, 25), 2);   // s0@2: 50 total
    results.emplace_back("Beta", pair_at(20, 30), 2);   // s1@2: 50 total

    auto j = SandboxTestExecutor::build_summary(results, {1, 2}, "perf");
    ASSERT_TRUE(j.contains("scalability"));
    // tp_total = 100, t1_total = 200 -> speedup = 2 at threads=2.
    EXPECT_DOUBLE_EQ(j["scalability"][1]["totalTimeMs"].get<double>(), 100.0);
    EXPECT_DOUBLE_EQ(j["scalability"][1]["speedup"].get<double>(), 2.0);
    EXPECT_DOUBLE_EQ(j["scalability"][1]["efficiency"].get<double>(), 1.0);
}

TEST(SandboxSummary, scalability_records_per_thread_max_rss) {
    // maxRssKb should track the per-thread-count peak across all tests at that thread count.
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{
            mk(true, 1.0, 100),
            mk(true, 1.0, 200)
        },
        1
    );
    results.emplace_back(
        "S",
        std::vector<TestResult>{
            mk(true, 1.0, 500),
            mk(true, 1.0, 300)
        },
        4
    );
    auto j = SandboxTestExecutor::build_summary(results, {1, 4}, "perf");
    EXPECT_EQ(j["scalability"][0]["maxRssKb"].get<long long>(), 200);
    EXPECT_EQ(j["scalability"][1]["maxRssKb"].get<long long>(), 500);
}
