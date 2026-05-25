#include <delaunay_test_common.h>

namespace {

    using delaunay_common::random_input;
    using delaunay_common::check_delaunay;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"n10",   random_input(algo, 10),   check_delaunay()},
            {"n50",   random_input(algo, 50),   check_delaunay()},
            {"n200",  random_input(algo, 200),  check_delaunay()},
            {"n500",  random_input(algo, 500),  check_delaunay()},
            {"n1k",   random_input(algo, 1000), check_delaunay()}
        };
    }

} // namespace

class BowyerWatsonCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("bowyer_watson");
        }
        std::string name() const override {
            return "Correctness.BowyerWatson";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(BowyerWatsonCorrectness)
