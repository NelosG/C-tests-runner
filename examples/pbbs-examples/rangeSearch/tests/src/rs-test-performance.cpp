#include <rs_test_common.h>

namespace {

    using rs_common::random_input;
    using rs_common::nonempty_header;

    // Single large SIFT-like scenario. HCNNG build dominates at high
    // dim; n=20k corpus + 2k queries at d=128 gives threads=1
    // baseline around 30-60 s.
    std::vector<Test> tests() {
        return {
            {"perf_d128_n12k_q1500", random_input(12'000, 1'500, 128, 3.0), nonempty_header()}
        };
    }

} // namespace

class HCNNGRangePerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override { return tests(); }
        std::string name() const override { return "Performance.HCNNGRange"; }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(HCNNGRangePerf)
