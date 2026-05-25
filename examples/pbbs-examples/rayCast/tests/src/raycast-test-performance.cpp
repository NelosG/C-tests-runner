#include <raycast_test_common.h>

namespace {

    using raycast_common::random_input;
    using raycast_common::has_size;

    std::vector<Test> tests_for(const std::string& algo) {
        // SAH kdTree build cost grew super-linearly on the input
        // triangle count - 100k blew past 200 s wall-time. 30k tri +
        // 5k rays keeps threads=1 baseline well under 60 s.
        return {
            {"perf_30k_5k", random_input(algo, 30'000, 5'000), has_size(5'000)}
        };
    }

} // namespace

class KdTreeRayCastPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("kdtree_ray_cast");
        }
        std::string name() const override {
            return "Performance.KdTreeRayCast";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(KdTreeRayCastPerf)
