#include <ann_test_common.h>

namespace {

    using ann_common::random_input;
    using ann_common::has_size;

    // Single large SIFT-like scenario. HCNNG build dominates wall-
    // clock at high dim; n=20k d=128 gives threads=1 baseline around
    // 30-60 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_d128_k10_n7k", random_input(algo, 10, 128, 7'000), has_size(7'000 * 10)}
        };
    }

} // namespace

class HCNNGPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("hcnng_ann");
        }
        std::string name() const override {
            return "Performance.HCNNG";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(HCNNGPerf)
