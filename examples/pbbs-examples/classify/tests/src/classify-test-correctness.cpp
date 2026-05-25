#include <classify_test_common.h>

namespace {

    using classify_common::copy_feature_input;
    using classify_common::accuracy_at_least;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            // Label = f1 with 10% label noise; tree should pick f1.
            {"copy_3vals_3feat",  copy_feature_input(algo, 3, 1000, 200, 3),  accuracy_at_least(0.80)},
            {"copy_4vals_3feat",  copy_feature_input(algo, 3, 1000, 200, 4),  accuracy_at_least(0.80)},
            // Add noise columns - learner still finds the right one.
            {"copy_3vals_5feat",  copy_feature_input(algo, 5, 1500, 300, 3),  accuracy_at_least(0.75)},
            // Larger training set
            {"copy_3vals_5k",     copy_feature_input(algo, 3, 5000, 500, 3),  accuracy_at_least(0.85)}
        };
    }

} // namespace

class DecisionTreeCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("decision_tree");
        }
        std::string name() const override {
            return "Correctness.DecisionTree";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(DecisionTreeCorrectness)
