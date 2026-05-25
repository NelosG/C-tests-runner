#include <nn_test_common.h>

namespace {

    using nn_common::random_input;
    using nn_common::check_knn;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"2d_k1_n100",  random_input(algo, 1,  2, 100),  check_knn()},
            {"2d_k5_n200",  random_input(algo, 5,  2, 200),  check_knn()},
            {"3d_k5_n200",  random_input(algo, 5,  3, 200),  check_knn()},
            {"2d_k10_n1k",  random_input(algo, 10, 2, 1000), check_knn()},
            {"3d_k10_n1k",  random_input(algo, 10, 3, 1000), check_knn()}
        };
    }

} // namespace

class OctTreeKNNCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("octtree_knn");
        }
        std::string name() const override {
            return "Correctness.OctTreeKNN";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(OctTreeKNNCorrectness)
