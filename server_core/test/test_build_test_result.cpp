// Unit tests for SandboxTestExecutor::build_test_result.
//
// build_test_result turns a single sandbox RunResult + verify() outcome into
// a populated TestResult. Pure post-processing, no sandbox is ever launched.
// We build RunResult / output_json / TestData / Test in memory and assert on
// the produced TestResult fields and message.

#include <gtest/gtest.h>

#include <sandbox_test_executor.h>
#include <test.h>
#include <test_data.h>

#include "test_temp_dir.h"

namespace {

    SandboxLauncher::RunResult make_run_result() {
        SandboxLauncher::RunResult r;
        r.exit_code = 0;
        r.timed_out = false;
        r.oom_killed = false;
        r.wall_time_sec = 0.0;
        r.cpu_time_sec = 0.0;
        r.cg_mem_peak_kb = 0;
        r.max_rss_kb = 0;
        return r;
    }

    Test make_passing_test() {
        return Test(
            "dummy",
            [](TestData&) {},
            [](const TestData&, const TestData&) {
                return std::make_pair(true, std::string("ok"));
            }
        );
    }

    Test make_failing_test() {
        return Test(
            "dummy",
            [](TestData&) {},
            [](const TestData&, const TestData&) {
                return std::make_pair(false, std::string("verify-says-no"));
            }
        );
    }

    Test make_throwing_test() {
        return Test(
            "dummy",
            [](TestData&) {},
            [](const TestData&, const TestData&) -> std::pair<bool, std::string> {
                throw std::runtime_error("kaboom");
            }
        );
    }

} // namespace

// -----------------------------------------------------------------------------
// Failure-mode classification - timeout, oom, exit_code != 0
// -----------------------------------------------------------------------------

TEST(BuildTestResult, timed_out_produces_time_limit_exceeded_message) {
    auto rr = make_run_result();
    rr.timed_out = true;
    rr.wall_time_sec = 60.0;
    rr.exit_code = -9;  // also set crash signal - timeout should take precedence
    TestData input;
    TempDir out;
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_EQ(tr.message, "Time limit exceeded");
    EXPECT_TRUE(tr.timed_out);
    EXPECT_DOUBLE_EQ(tr.wall_time_sec, 60.0);
}

TEST(BuildTestResult, oom_killed_produces_out_of_memory_message) {
    auto rr = make_run_result();
    rr.oom_killed = true;
    rr.cg_mem_peak_kb = 2 * 1024 * 1024;
    rr.exit_code = 137;  // OOM signal - OOM check should take precedence
    TestData input;
    TempDir out;
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_EQ(tr.message, "Out of memory");
    EXPECT_TRUE(tr.oom_killed);
    EXPECT_EQ(tr.cg_mem_peak_kb, 2 * 1024 * 1024);
}

TEST(BuildTestResult, positive_exit_code_says_process_exited) {
    auto rr = make_run_result();
    rr.exit_code = 42;
    TestData input;
    TempDir out;
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_EQ(tr.message, "Process exited with code 42");
    EXPECT_EQ(tr.exit_code, 42);
}

TEST(BuildTestResult, negative_exit_code_says_process_crashed_with_signal) {
    auto rr = make_run_result();
    rr.exit_code = -11;  // SIGSEGV-like
    TestData input;
    TempDir out;
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_EQ(tr.message, "Process crashed (signal 11)");
    EXPECT_EQ(tr.exit_code, -11);
}

// -----------------------------------------------------------------------------
// Output-missing detection
// -----------------------------------------------------------------------------

TEST(BuildTestResult, no_json_and_no_output_bin_yields_no_output_message) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;  // empty directory - no output.bin
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_EQ(tr.message, "Runner produced no output");
}

TEST(BuildTestResult, no_json_but_output_bin_exists_falls_through_to_verify) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_TRUE(tr.passed);
    EXPECT_EQ(tr.message, "ok");
}

// -----------------------------------------------------------------------------
// Resource fields propagate from RunResult into TestResult
// -----------------------------------------------------------------------------

TEST(BuildTestResult, resource_fields_propagate_from_run_result) {
    auto rr = make_run_result();
    rr.wall_time_sec = 3.5;
    rr.cpu_time_sec = 12.5;
    rr.max_rss_kb = 88000;
    rr.cg_mem_peak_kb = 92000;
    rr.stderr_output = "warning: foo";
    rr.exit_code = 1;  // short-circuit on exit code
    TestData input;
    TempDir out;
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    EXPECT_DOUBLE_EQ(tr.wall_time_sec, 3.5);
    EXPECT_DOUBLE_EQ(tr.cpu_time_sec, 12.5);
    EXPECT_EQ(tr.max_rss_kb, 88000);
    EXPECT_EQ(tr.cg_mem_peak_kb, 92000);
    EXPECT_EQ(tr.stderr_output, "warning: foo");
}

TEST(BuildTestResult, stderr_is_sanitized_through_path_sanitizer) {
    auto rr = make_run_result();
    rr.stderr_output = "error in /home/user/project/some_file.cpp:42";
    rr.exit_code = 1;
    TestData input;
    TempDir out;
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, std::nullopt, input, out.path());
    // Path sanitizer collapses absolute paths to basenames.
    EXPECT_NE(tr.stderr_output.find("some_file.cpp"), std::string::npos);
    EXPECT_EQ(tr.stderr_output.find("/home/user/"), std::string::npos);
}

// -----------------------------------------------------------------------------
// JSON parsing - timeMs and parallelStats counters
// -----------------------------------------------------------------------------

TEST(BuildTestResult, time_ms_is_read_from_output_json) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    nlohmann::json j = {{"timeMs", 123.5}};
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, j, input, out.path());
    EXPECT_DOUBLE_EQ(tr.time_ms, 123.5);
}

TEST(BuildTestResult, parallel_stats_block_populates_all_counters) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    nlohmann::json j = {
        {"timeMs", 7.0},
        {"parallelStats", {
            {"parallelRegions", 3},
            {"tasksCreated", 17},
            {"maxThreadsUsed", 8},
            {"singleRegions", 2},
            {"taskWaits", 4},
            {"barriers", 5},
            {"criticals", 6},
            {"forLoops", 7},
            {"atomics", 8},
            {"sections", 9},
            {"masters", 10},
            {"ordered", 11},
            {"taskGroups", 12},
            {"simdConstructs", 13},
            {"cancels", 14},
            {"flushes", 15},
            {"taskYields", 16},
            {"workNs", static_cast<long long>(100000)},
            {"spanNs", static_cast<long long>(50000)}
        }}
    };
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, j, input, out.path());
    EXPECT_EQ(tr.parallel_regions, 3);
    EXPECT_EQ(tr.tasks_created, 17);
    EXPECT_EQ(tr.max_threads_used, 8);
    EXPECT_EQ(tr.single_regions, 2);
    EXPECT_EQ(tr.taskwaits, 4);
    EXPECT_EQ(tr.barriers, 5);
    EXPECT_EQ(tr.criticals, 6);
    EXPECT_EQ(tr.for_loops, 7);
    EXPECT_EQ(tr.atomics, 8);
    EXPECT_EQ(tr.sections, 9);
    EXPECT_EQ(tr.masters, 10);
    EXPECT_EQ(tr.ordered, 11);
    EXPECT_EQ(tr.taskgroups, 12);
    EXPECT_EQ(tr.simd_constructs, 13);
    EXPECT_EQ(tr.cancels, 14);
    EXPECT_EQ(tr.flushes, 15);
    EXPECT_EQ(tr.taskyields, 16);
    EXPECT_EQ(tr.work_ns, 100000);
    EXPECT_EQ(tr.span_ns, 50000);
}

TEST(BuildTestResult, missing_parallel_stats_leaves_counters_at_zero) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    nlohmann::json j = {{"timeMs", 5.0}};
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, j, input, out.path());
    EXPECT_EQ(tr.parallel_regions, 0);
    EXPECT_EQ(tr.tasks_created, 0);
    EXPECT_EQ(tr.work_ns, 0);
}

TEST(BuildTestResult, wall_clock_span_fallback_when_monitoring_was_off) {
    // NORMAL monitor mode -> no parallelStats -> work_ns=span_ns=0.
    // build_test_result fills span_ns from time_ms * 1e6 so consumers see *some*
    // critical-path estimate.
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    nlohmann::json j = {{"timeMs", 7.0}};
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, j, input, out.path());
    EXPECT_EQ(tr.work_ns, 0);
    EXPECT_EQ(tr.span_ns, static_cast<long long>(7.0 * 1e6));
}

TEST(BuildTestResult, span_fallback_skipped_when_monitor_supplied_nonzero) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    nlohmann::json j = {
        {"timeMs", 9999.0},
        {"parallelStats", {{"workNs", static_cast<long long>(123)},
                           {"spanNs", static_cast<long long>(456)}}}
    };
    auto tr = SandboxTestExecutor::build_test_result(
        make_passing_test(), rr, j, input, out.path());
    EXPECT_EQ(tr.work_ns, 123);
    EXPECT_EQ(tr.span_ns, 456);
}

// -----------------------------------------------------------------------------
// verify() outcomes
// -----------------------------------------------------------------------------

TEST(BuildTestResult, verify_failure_propagates_message) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    auto tr = SandboxTestExecutor::build_test_result(
        make_failing_test(), rr, nlohmann::json::object(), input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_EQ(tr.message, "verify-says-no");
}

TEST(BuildTestResult, verify_exception_is_caught_and_reported) {
    auto rr = make_run_result();
    TestData input;
    TempDir out;
    TestData empty_out;
    empty_out.save(out.path() / "output.bin");
    auto tr = SandboxTestExecutor::build_test_result(
        make_throwing_test(), rr, nlohmann::json::object(), input, out.path());
    EXPECT_FALSE(tr.passed);
    EXPECT_NE(tr.message.find("Verify threw exception"), std::string::npos);
    EXPECT_NE(tr.message.find("kaboom"), std::string::npos);
}
