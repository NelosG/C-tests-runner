// Correctness scenarios. Only one variant for now, but using the same
// macro pattern as comparisonSort so adding more later is one-line.

#include <histogram_test_common.h>

namespace {

    using histogram_common::fixed_input;
    using histogram_common::random_input;
    using histogram_common::matches_input_counts;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single_element",
             fixed_input(algo, {2}, 4),
             matches_input_counts()},
            {"all_same_bucket",
             fixed_input(algo, {3, 3, 3, 3, 3}, 4),
             matches_input_counts()},
            {"spread",
             fixed_input(algo, {0, 1, 2, 3, 0, 1, 2, 0, 1, 0}, 4),
             matches_input_counts()},
            {"random_10k_256buckets",
             random_input(algo, 10'000, 256),
             matches_input_counts()},
            {"random_100k_1024buckets",
             random_input(algo, 100'000, 1024),
             matches_input_counts()},
            {"random_1M_65536buckets",
             random_input(algo, 1'000'000, 65'536),
             matches_input_counts()}
        };
    }

} // namespace

class ParallelHistogramCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_histogram");
        }
        std::string name() const override {
            return "Correctness.ParallelHistogram";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelHistogramCorrectness)
