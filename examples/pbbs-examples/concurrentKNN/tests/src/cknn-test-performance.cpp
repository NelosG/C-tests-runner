#include <cknn_test_common.h>

namespace {

    using cknn_common::random_input;
    using cknn_common::has_size;

    // Single large scenario. 2M 3D points with k=10 lands near the
    // memory regime pbbs benches at; threads=1 baseline ~25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_3d_k10_3M", random_input(algo, 10, 3, 3'000'000), has_size(3'000'000 * 10)}
        };
    }

} // namespace

class OctTreeCKNNPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("octtree_knn");
        }
        std::string name() const override {
            return "Performance.OctTreeCKNN";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(OctTreeCKNNPerf)
