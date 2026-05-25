#include <range_test_common.h>

namespace {

    using range_common::random_input;
    using range_common::check_total;

    // pbbs splits a single point set into queries (first 2*num_q points)
    // and data (the rest). num_q ~ n_total/3 in pbbs's driver.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"110p_10q",  random_input(algo, 110,    10),  check_total()},
            {"1k_50q",    random_input(algo, 1100,   50),  check_total()},
            {"10k_100q",  random_input(algo, 10'200, 100), check_total()}
        };
    }

} // namespace

class ParallelRangeCountCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_range_count");
        }
        std::string name() const override {
            return "Correctness.ParallelRangeCount";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelRangeCountCorrectness)
