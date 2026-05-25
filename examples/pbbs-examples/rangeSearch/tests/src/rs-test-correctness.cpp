#include <rs_test_common.h>

namespace {

    using rs_common::random_input;
    using rs_common::recall_at_least;

    // HCNNG is approximate; pick radii that yield non-trivial truth set
    // sizes for the chosen (n, dim) so recall is a meaningful metric.
    std::vector<Test> tests() {
        return {
            {"d16_n500_q50",   random_input(500,  50, 16, 1.0), recall_at_least(0.5)},
            {"d32_n1k_q100",   random_input(1000, 100, 32, 1.5), recall_at_least(0.5)},
            {"d64_n2k_q200",   random_input(2000, 200, 64, 2.0), recall_at_least(0.4)}
        };
    }

} // namespace

class HCNNGRangeCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override { return tests(); }
        std::string name() const override { return "Correctness.HCNNGRange"; }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(HCNNGRangeCorrectness)
