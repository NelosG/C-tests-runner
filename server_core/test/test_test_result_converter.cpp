// Unit tests for TestResultConverter - builds the parallelStats / processStats
// JSON sub-objects from a TestResult.

#include <gtest/gtest.h>
#include <test_result.h>
#include <test_result_converter.h>

// -----------------------------------------------------------------------------
// parallel_stats_json
// -----------------------------------------------------------------------------

TEST(TestResultConverter, parallel_stats_emits_all_camel_case_keys) {
    TestResult tr("t", true, "", 1.0);
    auto j = TestResultConverter::parallel_stats_json(tr);

    // The orchestrator depends on this exact key set.
    for(const auto& k : {
            "parallelRegions",
            "tasksCreated",
            "maxThreadsUsed",
            "singleRegions",
            "taskWaits",
            "barriers",
            "criticals",
            "forLoops",
            "atomics",
            "sections",
            "masters",
            "ordered",
            "taskGroups",
            "simdConstructs",
            "cancels",
            "flushes",
            "taskYields",
            "workNs",
            "spanNs"
        }) {
        EXPECT_TRUE(j.contains(k)) << "missing key: " << k;
    }
}

TEST(TestResultConverter, parallel_stats_passes_field_values_through) {
    TestResult tr("t", true, "", 1.0);
    tr.parallel_regions = 7;
    tr.tasks_created = 13;
    tr.max_threads_used = 4;
    tr.work_ns = 1'000'000;
    tr.span_ns = 250'000;

    auto j = TestResultConverter::parallel_stats_json(tr);
    EXPECT_EQ(j["parallelRegions"].get<int>(), 7);
    EXPECT_EQ(j["tasksCreated"].get<int>(), 13);
    EXPECT_EQ(j["maxThreadsUsed"].get<int>(), 4);
    EXPECT_EQ(j["workNs"].get<long long>(), 1'000'000);
    EXPECT_EQ(j["spanNs"].get<long long>(), 250'000);
}

// (Defaults-to-zero is already implied by the POD's value-init contract and
// the pass-through test above - no separate test needed.)

// -----------------------------------------------------------------------------
// process_stats_json
// -----------------------------------------------------------------------------

TEST(TestResultConverter, process_stats_emits_expected_keys) {
    TestResult tr("t", true, "", 1.0);
    auto j = TestResultConverter::process_stats_json(tr);
    for(const auto& k : {
            "exitCode",
            "cgMemPeakKb",
            "maxRssKb",
            "cpuTimeSec",
            "wallTimeSec",
            "oomKilled",
            "timedOut"
        }) {
        EXPECT_TRUE(j.contains(k)) << "missing key: " << k;
    }
}

TEST(TestResultConverter, process_stats_passes_field_values_through) {
    TestResult tr("t", false, "boom", 12.5);
    tr.exit_code = 137;          // SIGKILL exit code
    tr.cg_mem_peak_kb = 2048;
    tr.max_rss_kb = 1024;
    tr.cpu_time_sec = 1.25;
    tr.wall_time_sec = 2.5;
    tr.oom_killed = true;
    tr.timed_out = false;

    auto j = TestResultConverter::process_stats_json(tr);
    EXPECT_EQ(j["exitCode"].get<int>(), 137);
    EXPECT_EQ(j["cgMemPeakKb"].get<long long>(), 2048);
    EXPECT_EQ(j["maxRssKb"].get<long long>(), 1024);
    EXPECT_DOUBLE_EQ(j["cpuTimeSec"].get<double>(), 1.25);
    EXPECT_DOUBLE_EQ(j["wallTimeSec"].get<double>(), 2.5);
    EXPECT_TRUE(j["oomKilled"].get<bool>());
    EXPECT_FALSE(j["timedOut"].get<bool>());
}
