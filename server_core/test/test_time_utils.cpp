// Sanity check for time_utils::now_iso8601 - the format string is fixed
// ("%Y-%m-%dT%H:%M:%SZ") and the orchestrator relies on it being parseable
// as an ISO-8601 instant. We just verify the structure here; clock value
// itself is out of scope.

#include <gtest/gtest.h>
#include <regex>
#include <time_utils.h>

TEST(NowIso8601, has_expected_iso8601_zulu_shape) {
    auto stamp = now_iso8601();
    // YYYY-MM-DDTHH:MM:SSZ - 20 chars exactly.
    EXPECT_EQ(stamp.size(), 20u) << stamp;
    static const std::regex shape(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)"
    );
    EXPECT_TRUE(std::regex_match(stamp, shape)) << stamp;
}
