#include <raycast_test_common.h>

namespace {

    using raycast_common::random_input;
    using raycast_common::check_raycast;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"50t_20r",   random_input(algo,  50,   20),  check_raycast()},
            {"200t_50r",  random_input(algo, 200,   50),  check_raycast()},
            {"1k_100r",   random_input(algo, 1000, 100),  check_raycast()}
        };
    }

} // namespace

class KdTreeRayCastCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("kdtree_ray_cast");
        }
        std::string name() const override {
            return "Correctness.KdTreeRayCast";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(KdTreeRayCastCorrectness)
