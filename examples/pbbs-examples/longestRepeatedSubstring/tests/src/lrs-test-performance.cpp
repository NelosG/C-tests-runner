#include <lrs_test_common.h>

namespace {

    using lrs_common::random_input;
    using lrs_common::well_formed;

    // Single large scenario. 20M chars - the suffix-array build and
    // doubling LCP scan both dominate; threads=1 baseline ~25-40 s.
    // RAM ~1 GB (suffix array + LCP arrays at int per char).
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_25M", random_input(algo, 25'000'000), well_formed()}
        };
    }

} // namespace

class DoublingLRSPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("doubling_lrs");
        }
        std::string name() const override {
            return "Performance.DoublingLRS";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(DoublingLRSPerf)
