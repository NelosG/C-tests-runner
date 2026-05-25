#include <nn_test_common.h>

namespace {

    using nn_common::random_input;
    using nn_common::has_size;

    // Single large scenario. 2M 3D points with k=10 - well within
    // pbbs's NN benchmark regime; threads=1 baseline ~25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_3d_k10_3M", random_input(algo, 10, 3, 3'000'000), has_size(3'000'000 * 10)}
        };
    }

} // namespace

class OctTreeKNNPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("octtree_knn");
        }
        std::string name() const override {
            return "Performance.OctTreeKNN";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(OctTreeKNNPerf)
