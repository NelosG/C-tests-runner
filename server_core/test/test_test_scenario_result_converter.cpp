// Unit tests for TestScenarioResultConverter::to_grouped_json.
//
// Takes a *flat* vector of per-thread-count scenario results (laid out as
// [s0@tc0, s1@tc0, ..., s0@tcN, s1@tcN, ...]) and produces a grouped JSON
// array shaped scenario -> test -> runs[per thread count].

#include <gtest/gtest.h>
#include <test_scenario_result.h>
#include <test_scenario_result_converter.h>


namespace {

    /// Build a TestResult shorthand for tests below.
    TestResult tr(
        const std::string& name,
        double time_ms,
        long long work_ns = 0,
        long long span_ns = 0,
        int tasks = 0,
        int max_threads = 0
    ) {
        TestResult r(name, true, "", time_ms);
        r.work_ns = work_ns;
        r.span_ns = span_ns;
        r.tasks_created = tasks;
        r.max_threads_used = max_threads;
        return r;
    }

} // namespace

// -----------------------------------------------------------------------------
// Empty / malformed inputs
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, empty_inputs_yield_empty_array) {
    auto j = TestScenarioResultConverter::to_grouped_json({}, {});
    EXPECT_TRUE(j.is_array());
    EXPECT_TRUE(j.empty());
}

TEST(ScenarioResultConverter, empty_results_yields_empty_array) {
    auto j = TestScenarioResultConverter::to_grouped_json({}, {1, 2, 4});
    EXPECT_TRUE(j.is_array());
    EXPECT_TRUE(j.empty());
}

TEST(ScenarioResultConverter, non_divisible_sizes_yields_empty_array) {
    // 3 results / 2 thread counts is not divisible - refuse to guess the layout.
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{tr("t", 1.0)}, 1);
    results.emplace_back("S", std::vector<TestResult>{tr("t", 1.0)}, 2);
    results.emplace_back("BAD", std::vector<TestResult>{tr("t", 1.0)}, 4);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1, 2});
    EXPECT_TRUE(j.is_array());
    EXPECT_TRUE(j.empty());
}

// -----------------------------------------------------------------------------
// Single scenario / single thread count
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, single_scenario_single_thread_baseline_shape) {
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "Scenario.A",
        std::vector<TestResult>{tr("test1", 10.0, /*work*/20'000'000, /*span*/10'000'000)},
        /*threads*/
        1
    );

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    ASSERT_EQ(j.size(), 1u);

    const auto& scen = j[0];
    EXPECT_EQ(scen["name"], "Scenario.A");
    ASSERT_TRUE(scen.contains("tests"));
    ASSERT_EQ(scen["tests"].size(), 1u);

    const auto& test_entry = scen["tests"][0];
    EXPECT_EQ(test_entry["name"], "test1");
    ASSERT_EQ(test_entry["runs"].size(), 1u);

    const auto& run = test_entry["runs"][0];
    EXPECT_EQ(run["threads"].get<int>(), 1);
    EXPECT_TRUE(run["passed"].get<bool>());

    // At threads=1, speedup must be 1.0; efficiency = 1.0/1 = 1.0.
    EXPECT_DOUBLE_EQ(run["stats"]["timeMs"].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(run["stats"]["speedup"].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(run["stats"]["efficiency"].get<double>(), 1.0);
    // parallelism = work_ns / span_ns = 2.0
    EXPECT_DOUBLE_EQ(run["stats"]["parallelism"].get<double>(), 2.0);

    EXPECT_TRUE(run.contains("parallelStats"));
    EXPECT_TRUE(run.contains("processStats"));
}

// -----------------------------------------------------------------------------
// Multi thread counts - speedup / efficiency formulas
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, speedup_and_efficiency_against_t1_baseline) {
    // Two thread counts {1, 4}, one scenario, one test.
    // t1=100ms, t4=25ms -> speedup=4.0, efficiency=4/4=1.0.
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{tr("t", 100.0)}, 1);
    results.emplace_back("S", std::vector<TestResult>{tr("t", 25.0)}, 4);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1, 4});
    ASSERT_EQ(j.size(), 1u);

    const auto& runs = j[0]["tests"][0]["runs"];
    ASSERT_EQ(runs.size(), 2u);

    EXPECT_DOUBLE_EQ(runs[0]["stats"]["speedup"].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(runs[0]["stats"]["efficiency"].get<double>(), 1.0);

    EXPECT_DOUBLE_EQ(runs[1]["stats"]["speedup"].get<double>(), 4.0);
    EXPECT_DOUBLE_EQ(runs[1]["stats"]["efficiency"].get<double>(), 1.0);
}

TEST(ScenarioResultConverter, zero_time_does_not_divide_by_zero) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{tr("t", 0.0)}, 1);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    // Source falls back to 0.0 when time_ms is non-positive.
    EXPECT_DOUBLE_EQ(j[0]["tests"][0]["runs"][0]["stats"]["speedup"].get<double>(), 0.0);
}

TEST(ScenarioResultConverter, parallelism_omitted_when_span_is_zero) {
    // work > 0 but span = 0 -> parallelism would divide by zero, must be absent.
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{tr("t", 1.0, /*work*/1'000'000, /*span*/0)},
        1
    );

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    const auto& stats = j[0]["tests"][0]["runs"][0]["stats"];
    EXPECT_FALSE(stats.contains("parallelism"));
    // workMs / spanMs / computeEfficiency are still emitted because work_ns > 0.
    EXPECT_TRUE(stats.contains("workMs"));
    EXPECT_TRUE(stats.contains("spanMs"));
}

// -----------------------------------------------------------------------------
// Multi scenario layout - flat indexing scenario_index * t + s
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, multi_scenario_keeps_per_scenario_grouping) {
    // 2 scenarios x 2 thread counts. Layout:
    //   [s0@1, s1@1, s0@2, s1@2]
    std::vector<TestScenarioResult> results;
    results.emplace_back("Alpha", std::vector<TestResult>{tr("t", 10.0)}, 1);
    results.emplace_back("Beta", std::vector<TestResult>{tr("t", 20.0)}, 1);
    results.emplace_back("Alpha", std::vector<TestResult>{tr("t", 5.0)}, 2);
    results.emplace_back("Beta", std::vector<TestResult>{tr("t", 10.0)}, 2);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1, 2});
    ASSERT_EQ(j.size(), 2u);

    EXPECT_EQ(j[0]["name"], "Alpha");
    EXPECT_EQ(j[1]["name"], "Beta");

    // Speedup on Alpha: 10 -> 5 => 2x
    EXPECT_DOUBLE_EQ(j[0]["tests"][0]["runs"][1]["stats"]["speedup"].get<double>(), 2.0);
    // Speedup on Beta: 20 -> 10 => 2x
    EXPECT_DOUBLE_EQ(j[1]["tests"][0]["runs"][1]["stats"]["speedup"].get<double>(), 2.0);
}

// -----------------------------------------------------------------------------
// Performance-mode metrics block
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, perf_mode_adds_scenario_metrics) {
    // 2 tests in the scenario, totals: t1 = 100+50 = 150; tp = 25+10 = 35.
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{tr("a", 100.0), tr("b", 50.0)},
        1
    );
    results.emplace_back(
        "S",
        std::vector<TestResult>{tr("a", 25.0), tr("b", 10.0)},
        4
    );

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1, 4}, true);
    ASSERT_EQ(j.size(), 1u);
    ASSERT_TRUE(j[0].contains("metrics"));

    const auto& m = j[0]["metrics"];
    EXPECT_DOUBLE_EQ(m["t1Ms"].get<double>(), 150.0);
    EXPECT_DOUBLE_EQ(m["tpMs"].get<double>(), 35.0);
    EXPECT_DOUBLE_EQ(m["speedup"].get<double>(), 150.0 / 35.0);
    EXPECT_DOUBLE_EQ(m["efficiency"].get<double>(), (150.0 / 35.0) / 4.0);
}

TEST(ScenarioResultConverter, perf_mode_omits_metrics_when_single_thread_count) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{tr("t", 1.0)}, 1);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1}, true);
    EXPECT_FALSE(j[0].contains("metrics"))
        << "scenario metrics require >= 2 thread counts (speed-up has no meaning otherwise)";
}

TEST(ScenarioResultConverter, non_perf_mode_omits_metrics) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{tr("t", 100.0)}, 1);
    results.emplace_back("S", std::vector<TestResult>{tr("t", 25.0)}, 4);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1, 4}, false);
    EXPECT_FALSE(j[0].contains("metrics"));
}

// -----------------------------------------------------------------------------
// Conditional stats - only present when the underlying counters are non-zero
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, monitor_metrics_absent_when_work_ns_zero) {
    // No monitoring (cilk/parlay/seq, or OpenMP in performance mode):
    // work_ns == 0 -> workMs / spanMs / parallelism / computeEfficiency / avgTaskWorkMs
    // must all be absent from stats.
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{tr("t", 10.0, /*work*/0, /*span*/0, /*tasks*/5, /*max*/4)},
        1
    );

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    const auto& stats = j[0]["tests"][0]["runs"][0]["stats"];
    EXPECT_FALSE(stats.contains("workMs"));
    EXPECT_FALSE(stats.contains("spanMs"));
    EXPECT_FALSE(stats.contains("parallelism"));
    EXPECT_FALSE(stats.contains("computeEfficiency"));
    EXPECT_FALSE(stats.contains("avgTaskWorkMs"));
    // timeMs / speedup / efficiency stay - they don't depend on monitoring.
    EXPECT_DOUBLE_EQ(stats["timeMs"].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(stats["speedup"].get<double>(), 1.0);
}

TEST(ScenarioResultConverter, avg_task_work_present_only_when_tasks_created) {
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{tr("t", 10.0, /*work*/5'000'000, /*span*/0, /*tasks*/0)},
        1
    );

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    EXPECT_FALSE(j[0]["tests"][0]["runs"][0]["stats"].contains("avgTaskWorkMs"))
        << "no tasks reported -> avgTaskWorkMs must be absent (would divide by zero)";
}

TEST(ScenarioResultConverter, avg_task_work_computed_when_tasks_created) {
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{tr("t", 10.0, /*work*/10'000'000, /*span*/0, /*tasks*/10)},
        1
    );

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    ASSERT_TRUE(j[0]["tests"][0]["runs"][0]["stats"].contains("avgTaskWorkMs"));
    // workMs = 10, tasks = 10 -> avg = 1ms
    EXPECT_DOUBLE_EQ(j[0]["tests"][0]["runs"][0]["stats"]["avgTaskWorkMs"].get<double>(), 1.0);
}

TEST(ScenarioResultConverter, load_balance_ratio_present_only_with_span_and_threads_and_tasks) {
    // All three preconditions must hold: span_ns > 0, max_threads_used > 1, tasks_created > 0.
    std::vector<TestScenarioResult> results;
    results.emplace_back(
        "S",
        std::vector<TestResult>{
            tr("missing-tasks", 10.0, 1'000'000, 100'000, 0, 4)
        },
        1
    );
    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    EXPECT_FALSE(j[0]["tests"][0]["runs"][0]["stats"].contains("loadBalanceRatio"));

    std::vector<TestScenarioResult> results2;
    results2.emplace_back(
        "S",
        std::vector<TestResult>{
            tr("ok", 10.0, /*work*/10'000'000, /*span*/2'500'000, /*tasks*/100, /*max*/4)
        },
        1
    );
    auto j2 = TestScenarioResultConverter::to_grouped_json(results2, {1});
    // load = work / (span * threads) = 10M / (2.5M * 4) = 1.0
    ASSERT_TRUE(j2[0]["tests"][0]["runs"][0]["stats"].contains("loadBalanceRatio"));
    EXPECT_DOUBLE_EQ(j2[0]["tests"][0]["runs"][0]["stats"]["loadBalanceRatio"].get<double>(), 1.0);
}

// -----------------------------------------------------------------------------
// Optional run fields
// -----------------------------------------------------------------------------

TEST(ScenarioResultConverter, empty_message_is_omitted) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{tr("t", 1.0)}, 1);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    EXPECT_FALSE(j[0]["tests"][0]["runs"][0].contains("message"));
}

TEST(ScenarioResultConverter, non_empty_message_is_passed_through) {
    TestResult failing("t", false, "expected 6, got 5", 1.0);
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{failing}, 1);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    EXPECT_EQ(j[0]["tests"][0]["runs"][0]["message"], "expected 6, got 5");
    EXPECT_FALSE(j[0]["tests"][0]["runs"][0]["passed"].get<bool>());
}

TEST(ScenarioResultConverter, stderr_output_passed_through_when_present) {
    TestResult with_err("t", true, "", 1.0);
    with_err.stderr_output = "warning: something";
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{with_err}, 1);

    auto j = TestScenarioResultConverter::to_grouped_json(results, {1});
    EXPECT_EQ(j[0]["tests"][0]["runs"][0]["stderrOutput"], "warning: something");
}
