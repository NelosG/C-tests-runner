// Unit tests for the enum / string conversions in api_types.h.

#include <api_types.h>
#include <gtest/gtest.h>

// -----------------------------------------------------------------------------
// test_mode
// -----------------------------------------------------------------------------

TEST(ApiTypesTestMode, to_string_round_trip) {
    EXPECT_EQ(to_string(test_mode::correctness), "correctness");
    EXPECT_EQ(to_string(test_mode::performance), "performance");
    EXPECT_EQ(to_string(test_mode::all), "all");
}

TEST(ApiTypesTestMode, from_string_round_trip) {
    EXPECT_EQ(test_mode_from_string("correctness"), test_mode::correctness);
    EXPECT_EQ(test_mode_from_string("performance"), test_mode::performance);
    EXPECT_EQ(test_mode_from_string("all"), test_mode::all);
}

TEST(ApiTypesTestMode, from_string_unknown_throws) {
    EXPECT_THROW(test_mode_from_string(""), std::invalid_argument);
    EXPECT_THROW(test_mode_from_string("Correctness"), std::invalid_argument);  // case-sensitive
    EXPECT_THROW(test_mode_from_string("perf"), std::invalid_argument);
    EXPECT_THROW(test_mode_from_string("any"), std::invalid_argument);
}

TEST(ApiTypesTestMode, is_valid_test_mode_matches_known) {
    EXPECT_TRUE(is_valid_test_mode("correctness"));
    EXPECT_TRUE(is_valid_test_mode("performance"));
    EXPECT_TRUE(is_valid_test_mode("all"));
    EXPECT_FALSE(is_valid_test_mode(""));
    EXPECT_FALSE(is_valid_test_mode("Correctness"));
    EXPECT_FALSE(is_valid_test_mode("any"));
}

// -----------------------------------------------------------------------------
// job_status
// -----------------------------------------------------------------------------

TEST(ApiTypesJobStatus, to_string_covers_all_states) {
    EXPECT_EQ(to_string(job_status::queued), "queued");
    EXPECT_EQ(to_string(job_status::building), "building");
    EXPECT_EQ(to_string(job_status::running), "running");
    EXPECT_EQ(to_string(job_status::completed), "completed");
    EXPECT_EQ(to_string(job_status::failed), "failed");
    EXPECT_EQ(to_string(job_status::cancelled), "cancelled");
}

TEST(ApiTypesJobStatus, from_string_round_trip) {
    for(auto s : {
            job_status::queued,
            job_status::building,
            job_status::running,
            job_status::completed,
            job_status::failed,
            job_status::cancelled
        }) {
        EXPECT_EQ(job_status_from_string(to_string(s)), s);
    }
}

TEST(ApiTypesJobStatus, from_string_unknown_throws) {
    EXPECT_THROW(job_status_from_string(""), std::invalid_argument);
    EXPECT_THROW(job_status_from_string("queue"), std::invalid_argument);
    EXPECT_THROW(job_status_from_string("DONE"), std::invalid_argument);
}

// queue_status / response_status have no from_string / validators and only
// 2-3 enum values each - their to_string switch is too trivial to be worth a
// dedicated test. Coverage comes implicitly through JobQueue.get_status.
