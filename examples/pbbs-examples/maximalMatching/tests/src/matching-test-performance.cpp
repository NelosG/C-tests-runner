#include <matching_test_common.h>

namespace {

    using matching_common::random_input;
    using matching_common::has_some_matching;

    // Single large random graph. Per-variant threads=1 baseline lands
    // around 20-30 s. 5M vertices, 30M edges - CSR ~280 MB.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_8M_50M", random_input(algo, 8'000'000, 50'000'000), has_some_matching()}
        };
    }

} // namespace

#define MATCH_VARIANT_PERF(ClassName, scenario_label, algo_key)           \
    class ClassName final : public TestScenarioExtension {                \
        public:                                                           \
            std::vector<Test> get_tests() const override {                \
                return tests_for(algo_key);                               \
            }                                                             \
            std::string name() const override {                           \
                return "Performance." scenario_label;                     \
            }                                                             \
            ScenarioType scenario_type() const override {                 \
                return ScenarioType::PERFORMANCE;                         \
            }                                                             \
    };                                                                    \
    REGISTER_TEST(ClassName)

MATCH_VARIANT_PERF(NdMatchingPerf,          "NdMatching",          "nd_matching")
MATCH_VARIANT_PERF(IncrementalMatchingPerf, "IncrementalMatching", "incremental_matching")
