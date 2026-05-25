// Unit tests for Pipeline's pure static helpers and JobContext methods.
//
// We do NOT build a Pipeline instance here (that requires real BuildService /
// SandboxTestExecutor / ResourceManager). The helpers below are all `static`
// or live on the public JobContext struct - they have no service dependencies
// and operate on JSON / time / vectors directly. Full Pipeline orchestration
// is exercised by integration runs in examples/.

#include <gtest/gtest.h>

#include <pipeline.h>
#include <test_result.h>
#include <test_scenario_result.h>

namespace {

    Pipeline::JobContext make_ctx(const nlohmann::json& req) {
        Pipeline::JobContext ctx{req};
        ctx.job_id = "test-job";
        ctx.node_id = "node-1";
        return ctx;
    }

    TestResult mk_test_result(bool passed) {
        return TestResult("t", passed, "", 1.0);
    }

} // namespace

// -----------------------------------------------------------------------------
// JobContext::add_step + step_ms
// -----------------------------------------------------------------------------

TEST(PipelineJobContext, add_step_appends_to_pipeline_array_with_camelcase_keys) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    ctx.add_step("resolve", "ok", 1.5);
    ASSERT_EQ(ctx.pipeline.size(), 1u);
    EXPECT_EQ(ctx.pipeline[0]["step"].get<std::string>(), "resolve");
    EXPECT_EQ(ctx.pipeline[0]["status"].get<std::string>(), "ok");
    EXPECT_DOUBLE_EQ(ctx.pipeline[0]["durationMs"].get<double>(), 1.5);
}

TEST(PipelineJobContext, add_step_preserves_insertion_order) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    ctx.add_step("a", "ok", 1.0);
    ctx.add_step("b", "failed", 2.0);
    ctx.add_step("c", "ok", 3.0);
    ASSERT_EQ(ctx.pipeline.size(), 3u);
    EXPECT_EQ(ctx.pipeline[0]["step"].get<std::string>(), "a");
    EXPECT_EQ(ctx.pipeline[1]["step"].get<std::string>(), "b");
    EXPECT_EQ(ctx.pipeline[2]["step"].get<std::string>(), "c");
}

TEST(PipelineJobContext, step_ms_returns_non_negative_elapsed_and_resets_clock) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    // First call: elapsed since job_start (typically ~0 here).
    double first = ctx.step_ms();
    EXPECT_GE(first, 0.0);
    // Second call: elapsed since first call (clock reset by step_ms).
    double second = ctx.step_ms();
    EXPECT_GE(second, 0.0);
    // The second call's reading should be tiny because we did no work between.
    EXPECT_LT(second, 1000.0);
}

// -----------------------------------------------------------------------------
// JobContext::emit_phase
// -----------------------------------------------------------------------------

TEST(PipelineJobContext, emit_phase_is_noop_when_callback_not_set) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    // No on_progress installed - this must NOT throw.
    EXPECT_NO_THROW(ctx.emit_phase("buildRunner", "msg", 0.5));
}

TEST(PipelineJobContext, emit_phase_invokes_callback_with_camelcase_payload) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    nlohmann::json received;
    ctx.on_progress = [&received](const nlohmann::json& ev) { received = ev; };
    ctx.emit_phase("buildRunner", "Compiling solution", 0.25);
    EXPECT_EQ(received["jobId"].get<std::string>(), "test-job");
    EXPECT_EQ(received["nodeId"].get<std::string>(), "node-1");
    EXPECT_EQ(received["phase"].get<std::string>(), "buildRunner");
    EXPECT_EQ(received["message"].get<std::string>(), "Compiling solution");
    EXPECT_DOUBLE_EQ(received["progress"].get<double>(), 0.25);
    EXPECT_TRUE(received.contains("timestamp"));
}

TEST(PipelineJobContext, emit_phase_omits_message_and_progress_when_absent) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    nlohmann::json received;
    ctx.on_progress = [&received](const nlohmann::json& ev) { received = ev; };
    ctx.emit_phase("loadPlugins");
    EXPECT_FALSE(received.contains("message"));
    EXPECT_FALSE(received.contains("progress"));
    EXPECT_EQ(received["phase"].get<std::string>(), "loadPlugins");
}

TEST(PipelineJobContext, emit_phase_swallows_exceptions_from_callback) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    ctx.on_progress = [](const nlohmann::json&) {
        throw std::runtime_error("listener exploded");
    };
    // Pipeline must never let a flaky listener break a job.
    EXPECT_NO_THROW(ctx.emit_phase("validation"));
}

// -----------------------------------------------------------------------------
// Pipeline::fail_step - uniform failure path
// -----------------------------------------------------------------------------

TEST(PipelineFailStep, returns_false_for_chained_use) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    EXPECT_FALSE(Pipeline::fail_step(ctx, "validation", "boom"));
}

TEST(PipelineFailStep, populates_result_with_failed_status_and_error_details) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    Pipeline::fail_step(ctx, "validation", "Forbidden dependency: openssl",
                        {{"violations", {"openssl"}}});
    EXPECT_EQ(ctx.result["status"].get<std::string>(), "failed");
    EXPECT_EQ(ctx.result["failedStep"].get<std::string>(), "validation");
    EXPECT_EQ(ctx.result["error"].get<std::string>(), "Forbidden dependency: openssl");
    EXPECT_EQ(ctx.result["errorDetails"]["step"].get<std::string>(), "validation");
    EXPECT_EQ(ctx.result["errorDetails"]["violations"][0].get<std::string>(), "openssl");
}

TEST(PipelineFailStep, records_step_in_pipeline_array_with_failed_status) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    Pipeline::fail_step(ctx, "buildRunner", "compile error");
    ASSERT_EQ(ctx.pipeline.size(), 1u);
    EXPECT_EQ(ctx.pipeline[0]["step"].get<std::string>(), "buildRunner");
    EXPECT_EQ(ctx.pipeline[0]["status"].get<std::string>(), "failed");
}

TEST(PipelineFailStep, non_object_extra_details_falls_back_to_empty_object) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    // Pass an array instead of an object - fail_step must coerce to {}.
    Pipeline::fail_step(ctx, "resolve", "no path", nlohmann::json::array({1, 2}));
    EXPECT_TRUE(ctx.result["errorDetails"].is_object());
    EXPECT_EQ(ctx.result["errorDetails"]["step"].get<std::string>(), "resolve");
}

// -----------------------------------------------------------------------------
// Pipeline::all_tests_passed
// -----------------------------------------------------------------------------

TEST(PipelineAllTestsPassed, returns_true_for_empty_results) {
    std::vector<TestScenarioResult> results;
    EXPECT_TRUE(Pipeline::all_tests_passed(results));
}

TEST(PipelineAllTestsPassed, returns_true_when_every_test_passed) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{mk_test_result(true),
                                                       mk_test_result(true)}, 1);
    EXPECT_TRUE(Pipeline::all_tests_passed(results));
}

TEST(PipelineAllTestsPassed, returns_false_when_any_test_failed) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S", std::vector<TestResult>{mk_test_result(true),
                                                       mk_test_result(false)}, 1);
    EXPECT_FALSE(Pipeline::all_tests_passed(results));
}

TEST(PipelineAllTestsPassed, scans_across_multiple_scenarios) {
    std::vector<TestScenarioResult> results;
    results.emplace_back("S0", std::vector<TestResult>{mk_test_result(true)}, 1);
    results.emplace_back("S1", std::vector<TestResult>{mk_test_result(false)}, 1);
    EXPECT_FALSE(Pipeline::all_tests_passed(results));
}

// -----------------------------------------------------------------------------
// Pipeline::emit_lane_result - assembles lane block + threadCounts entry
// -----------------------------------------------------------------------------

TEST(PipelineEmitLaneResult, writes_lane_key_thread_counts_and_summary) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    Pipeline::LaneResults lane;
    lane.thread_counts = {1, 2};
    lane.results.emplace_back("S",
        std::vector<TestResult>{mk_test_result(true)}, 1);
    lane.results.emplace_back("S",
        std::vector<TestResult>{mk_test_result(true)}, 2);
    nlohmann::json summary;

    Pipeline::emit_lane_result(ctx, "correctness", false, lane, summary);
    EXPECT_TRUE(ctx.result.contains("correctness"));
    EXPECT_TRUE(ctx.result["threadCounts"].is_object());
    ASSERT_TRUE(ctx.result["threadCounts"].contains("correctness"));
    EXPECT_EQ(ctx.result["threadCounts"]["correctness"][0].get<int>(), 1);
    EXPECT_EQ(ctx.result["threadCounts"]["correctness"][1].get<int>(), 2);
    EXPECT_TRUE(summary.contains("correctness"));
}

TEST(PipelineEmitLaneResult, preserves_existing_thread_counts_entry) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    ctx.result["threadCounts"] = {{"correctness", {1, 2}}};
    Pipeline::LaneResults lane;
    lane.thread_counts = {1, 4};
    lane.results.emplace_back("S",
        std::vector<TestResult>{mk_test_result(true)}, 1);
    lane.results.emplace_back("S",
        std::vector<TestResult>{mk_test_result(true)}, 4);
    nlohmann::json summary;

    Pipeline::emit_lane_result(ctx, "performance", true, lane, summary);
    // Both keys present after the second call.
    EXPECT_TRUE(ctx.result["threadCounts"].contains("correctness"));
    EXPECT_TRUE(ctx.result["threadCounts"].contains("performance"));
}

TEST(PipelineEmitLaneResult, replaces_non_object_thread_counts_value) {
    nlohmann::json req = {{"jobId", "j"}, {"testId", "t"}};
    auto ctx = make_ctx(req);
    // Defensive: someone stuffed a non-object into result["threadCounts"].
    ctx.result["threadCounts"] = "garbage";
    Pipeline::LaneResults lane;
    lane.thread_counts = {1};
    lane.results.emplace_back("S",
        std::vector<TestResult>{mk_test_result(true)}, 1);
    nlohmann::json summary;
    Pipeline::emit_lane_result(ctx, "correctness", false, lane, summary);
    EXPECT_TRUE(ctx.result["threadCounts"].is_object());
}
