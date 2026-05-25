#include <ann_test_common.h>

namespace {

    using ann_common::random_input;
    using ann_common::recall_at_least;

    // pbbs's ANN bench targets high-dim float/uint8 vectors (SIFT 128d,
    // BIGANN). We keep the dimensionality regime but cap n so a full
    // scalability sweep finishes in tens of seconds - the n^2 brute-
    // force recall verifier dominates wall-clock at larger n.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"d16_k10_n500", random_input(algo, 10, 16, 500),  recall_at_least(0.70)},
            {"d32_k10_n1k",  random_input(algo, 10, 32, 1000), recall_at_least(0.65)},
            {"d64_k10_n1k",  random_input(algo, 10, 64, 1000), recall_at_least(0.60)}
        };
    }

} // namespace

class HCNNGCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("hcnng_ann");
        }
        std::string name() const override {
            return "Correctness.HCNNG";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(HCNNGCorrectness)
