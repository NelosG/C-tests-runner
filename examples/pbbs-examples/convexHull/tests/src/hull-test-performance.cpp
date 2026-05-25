#include <hull_test_common.h>

namespace {

    using hull_common::random_input;
    using hull_common::has_some_hull;

    // Single large scenario. 50M 2D points (~800 MB input) is the
    // regime pbbs ConvexHull benchmark targets; threads=1 baseline
    // lands around 25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_100M", random_input(algo, 100'000'000), has_some_hull()}
        };
    }

} // namespace

class ParallelQuickHullPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_quick_hull");
        }
        std::string name() const override {
            return "Performance.ParallelQuickHull";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelQuickHullPerf)
