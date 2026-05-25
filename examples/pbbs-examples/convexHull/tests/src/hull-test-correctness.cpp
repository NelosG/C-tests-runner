#include <hull_test_common.h>

namespace {

    using hull_common::fixed_input;
    using hull_common::random_input;
    using hull_common::check_hull;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"triangle",      fixed_input(algo, {0.0, 1.0, 0.5}, {0.0, 0.0, 1.0}),         check_hull()},
            {"square",        fixed_input(algo, {0.0, 1.0, 1.0, 0.0}, {0.0, 0.0, 1.0, 1.0}), check_hull()},
            {"square_w_interior",
                              fixed_input(algo,
                                {0.0, 1.0, 1.0, 0.0, 0.5, 0.3},
                                {0.0, 0.0, 1.0, 1.0, 0.5, 0.7}),                            check_hull()},
            {"random_1k",     random_input(algo, 1000),                                     check_hull()},
            {"random_10k",    random_input(algo, 10'000),                                   check_hull()},
            {"random_100k",   random_input(algo, 100'000),                                  check_hull()}
        };
    }

} // namespace

class ParallelQuickHullCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_quick_hull");
        }
        std::string name() const override {
            return "Correctness.ParallelQuickHull";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelQuickHullCorrectness)
