#include <classify_test_common.h>

namespace {

    using classify_common::copy_feature_input;
    using classify_common::has_size;

    // Single large scenario. 1M training rows over 10 features and
    // 50k test rows; decision-tree build dominates so threads=1
    // baseline lands around 20-30 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_20feat_15M", copy_feature_input(algo, 20, 15'000'000, 500'000, 4), has_size(500'000)}
        };
    }

} // namespace

class DecisionTreePerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("decision_tree");
        }
        std::string name() const override {
            return "Performance.DecisionTree";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(DecisionTreePerf)
