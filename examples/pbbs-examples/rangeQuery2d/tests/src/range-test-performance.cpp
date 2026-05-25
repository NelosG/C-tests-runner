#include <range_test_common.h>

namespace {

    using range_common::random_input;
    using range_common::is_nonneg;

    // Single large scenario. Our simplified algorithm is O(N) per
    // query (no PAM), so total work is O(N*Q). 2M points + 5k
    // queries -> 10G compare ops; threads=1 baseline ~25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_2M_4k", random_input(algo, 2'008'000, 4'000), is_nonneg()}
        };
    }

} // namespace

class ParallelRangeCountPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_range_count");
        }
        std::string name() const override {
            return "Performance.ParallelRangeCount";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelRangeCountPerf)
