#include <refine_test_common.h>

namespace {

    using refine_common::random_input;
    using refine_common::check_refine;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"n20_ang20",   random_input(algo, 20,  20.0), check_refine()},
            {"n50_ang25",   random_input(algo, 50,  25.0), check_refine()},
            {"n100_ang20",  random_input(algo, 100, 20.0), check_refine()},
            {"n200_ang20",  random_input(algo, 200, 20.0), check_refine()}
        };
    }

} // namespace

class IncrementalRefineCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("incremental_refine");
        }
        std::string name() const override {
            return "Correctness.IncrementalRefine";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(IncrementalRefineCorrectness)
