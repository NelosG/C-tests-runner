#include <nbody_test_common.h>

namespace {

    using nbody_common::random_input;
    using nbody_common::has_size;

    // Single large scenario. pbbs CK n-body builds a tree so the
    // cost is closer to O(n log n) than naive O(n^2); 50k particles
    // lands threads=1 baseline near 25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_1M", random_input(algo, 1'000'000), has_size(3'000'000)}
        };
    }

} // namespace

class AllPairsNBodyPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("all_pairs_nbody");
        }
        std::string name() const override {
            return "Performance.AllPairsNBody";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(AllPairsNBodyPerf)
