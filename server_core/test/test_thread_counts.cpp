// Unit tests for ThreadCounts::get - the helper that picks which thread counts
// to test based on mode + machine capacity.
//
// Contract (from thread_counts.h):
//   - performance: {1, max} when max>1, else {1}
//   - correctness/all: starts with 1; adds 2, 4 if available; appends max iff max>4

#include <gtest/gtest.h>
#include <thread_counts.h>

// -----------------------------------------------------------------------------
// performance mode - exactly two anchors for speed-up measurement
// -----------------------------------------------------------------------------

TEST(ThreadCountsPerformance, single_core_box_falls_back_to_one) {
    EXPECT_EQ(ThreadCounts::get("performance", 1), std::vector<int>({1}));
}

TEST(ThreadCountsPerformance, multi_core_returns_one_and_max) {
    EXPECT_EQ(ThreadCounts::get("performance", 2), std::vector<int>({1, 2}));
    EXPECT_EQ(ThreadCounts::get("performance", 4), std::vector<int>({1, 4}));
    EXPECT_EQ(ThreadCounts::get("performance", 16), std::vector<int>({1, 16}));
}

// -----------------------------------------------------------------------------
// correctness / all - geometric ladder up to the machine limit
// -----------------------------------------------------------------------------

TEST(ThreadCountsCorrectness, max_one_yields_only_one) {
    EXPECT_EQ(ThreadCounts::get("correctness", 1), std::vector<int>({1}));
    EXPECT_EQ(ThreadCounts::get("all", 1), std::vector<int>({1}));
}

TEST(ThreadCountsCorrectness, max_two_adds_two) {
    EXPECT_EQ(ThreadCounts::get("correctness", 2), std::vector<int>({1, 2}));
}

TEST(ThreadCountsCorrectness, max_three_does_not_add_four) {
    // 4 is conditional on max>=4 - at max=3 we should only have {1, 2}.
    EXPECT_EQ(ThreadCounts::get("correctness", 3), std::vector<int>({1, 2}));
}

TEST(ThreadCountsCorrectness, max_four_includes_four_without_duplicating_max) {
    // max==4 satisfies "add 4" but not "max>4", so the trailing append is
    // suppressed - otherwise we'd see {1,2,4,4}.
    EXPECT_EQ(ThreadCounts::get("correctness", 4), std::vector<int>({1, 2, 4}));
}

TEST(ThreadCountsCorrectness, max_above_four_appends_machine_max) {
    EXPECT_EQ(ThreadCounts::get("correctness", 8), std::vector<int>({1, 2, 4, 8}));
    EXPECT_EQ(ThreadCounts::get("correctness", 16), std::vector<int>({1, 2, 4, 16}));
    EXPECT_EQ(ThreadCounts::get("correctness", 32), std::vector<int>({1, 2, 4, 32}));
}

TEST(ThreadCountsCorrectness, all_mode_behaves_like_correctness) {
    EXPECT_EQ(ThreadCounts::get("all", 8), ThreadCounts::get("correctness", 8));
    EXPECT_EQ(ThreadCounts::get("all", 16), ThreadCounts::get("correctness", 16));
}

// -----------------------------------------------------------------------------
// Mode normalisation - only "performance" takes the perf branch
// -----------------------------------------------------------------------------

TEST(ThreadCountsModeHandling, unknown_mode_falls_into_correctness_branch) {
    // "" / "PERFORMANCE" / random strings are not equal to to_string(performance),
    // so they hit the correctness/all branch.
    EXPECT_EQ(ThreadCounts::get("", 4), ThreadCounts::get("correctness", 4));
    EXPECT_EQ(ThreadCounts::get("PERFORMANCE", 4), ThreadCounts::get("correctness", 4));
    EXPECT_EQ(ThreadCounts::get("perf", 4), ThreadCounts::get("correctness", 4));
}

TEST(ThreadCountsModeHandling, first_element_is_always_one) {
    // The first slot is the baseline for speedup / efficiency calculations
    // (see TestScenarioResultConverter). Drift here would corrupt all metrics.
    for(int n : {1, 2, 3, 4, 5, 8, 16, 64}) {
        EXPECT_EQ(ThreadCounts::get("correctness", n).front(), 1) << "n=" << n;
        EXPECT_EQ(ThreadCounts::get("performance", n).front(), 1) << "n=" << n;
    }
}
