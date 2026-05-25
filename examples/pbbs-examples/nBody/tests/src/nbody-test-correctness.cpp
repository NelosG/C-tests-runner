#include <nbody_test_common.h>

namespace {

    using nbody_common::random_input;
    using nbody_common::check_forces;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"n50",  random_input(algo,  50),  check_forces()},
            {"n200", random_input(algo, 200),  check_forces()},
            {"n500", random_input(algo, 500),  check_forces()},
            {"n1k",  random_input(algo, 1000), check_forces()}
        };
    }

} // namespace

class AllPairsNBodyCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("all_pairs_nbody");
        }
        std::string name() const override {
            return "Correctness.AllPairsNBody";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(AllPairsNBodyCorrectness)
