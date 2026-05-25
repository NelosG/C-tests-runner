#include <refine_test_common.h>

namespace {

    using refine_common::random_input;
    using refine_common::has_some_tris;

    // Single large scenario. The test plugin runs sequential Bowyer-
    // Watson O(n^2) in setup (untimed). n=15k -> ~5 s of setup; n=50k
    // -> ~60 s. We push to 35k as a compromise: setup ~30 s once,
    // refine body T=1 ~5 s, which still lands T=16 in the perf window
    // without making the host-side setup unbearable.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_100k", random_input(algo, 100'000, 20.0), has_some_tris()}
        };
    }

} // namespace

class IncrementalRefinePerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("incremental_refine");
        }
        std::string name() const override {
            return "Performance.IncrementalRefine";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(IncrementalRefinePerf)
