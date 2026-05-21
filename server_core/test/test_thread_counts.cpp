// Unit tests for ThreadCounts::get - powers-of-two ladder up to the machine
// limit, with max appended verbatim when it is not itself a power of two.

#include <gtest/gtest.h>
#include <thread_counts.h>

TEST(ThreadCounts, single_core_box_yields_only_one) {
    EXPECT_EQ(ThreadCounts::get(1), std::vector<int>({1}));
}

TEST(ThreadCounts, powers_of_two_form_a_pure_doubling_sequence) {
    EXPECT_EQ(ThreadCounts::get(2),  std::vector<int>({1, 2}));
    EXPECT_EQ(ThreadCounts::get(4),  std::vector<int>({1, 2, 4}));
    EXPECT_EQ(ThreadCounts::get(8),  std::vector<int>({1, 2, 4, 8}));
    EXPECT_EQ(ThreadCounts::get(16), std::vector<int>({1, 2, 4, 8, 16}));
    EXPECT_EQ(ThreadCounts::get(32), std::vector<int>({1, 2, 4, 8, 16, 32}));
}

TEST(ThreadCounts, non_power_of_two_max_gets_appended_after_the_ladder) {
    // 3 -> ladder reaches 2, then 3 is the actual ceiling
    EXPECT_EQ(ThreadCounts::get(3),  std::vector<int>({1, 2, 3}));
    // 7 -> ladder reaches 4, append 7
    EXPECT_EQ(ThreadCounts::get(7),  std::vector<int>({1, 2, 4, 7}));
    // 20 -> ladder reaches 16, append 20
    EXPECT_EQ(ThreadCounts::get(20), std::vector<int>({1, 2, 4, 8, 16, 20}));
    // 24 -> ladder reaches 16, append 24
    EXPECT_EQ(ThreadCounts::get(24), std::vector<int>({1, 2, 4, 8, 16, 24}));
}

TEST(ThreadCounts, zero_or_negative_max_is_clamped_to_one) {
    // Defensive: ctx.threads should never be < 1, but if it ever is we still
    // produce a usable single-core sequence rather than crash or empty out.
    EXPECT_EQ(ThreadCounts::get(0),  std::vector<int>({1}));
    EXPECT_EQ(ThreadCounts::get(-5), std::vector<int>({1}));
}

TEST(ThreadCounts, first_element_is_always_one) {
    // The first slot is the baseline for speedup / efficiency calculations
    // in TestScenarioResultConverter. Drift here would corrupt all metrics.
    for(int n : {1, 2, 3, 4, 5, 7, 8, 16, 20, 64}) {
        EXPECT_EQ(ThreadCounts::get(n).front(), 1) << "n=" << n;
    }
}

TEST(ThreadCounts, last_element_is_always_max) {
    // Whatever path we take, the last slot must equal the machine ceiling so
    // we measure peak parallelism.
    for(int n : {1, 2, 3, 4, 7, 8, 16, 20, 24, 64}) {
        EXPECT_EQ(ThreadCounts::get(n).back(), n) << "n=" << n;
    }
}

TEST(ThreadCounts, sequence_is_strictly_increasing) {
    for(int n : {1, 2, 3, 4, 5, 7, 8, 16, 20, 24, 64}) {
        auto v = ThreadCounts::get(n);
        for(size_t i = 1; i < v.size(); ++i) {
            EXPECT_LT(v[i - 1], v[i]) << "n=" << n << " at index " << i;
        }
    }
}
