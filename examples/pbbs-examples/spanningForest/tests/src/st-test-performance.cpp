#include <st_test_common.h>

namespace {

    using st_common::random_input;
    using st_common::has_some_st;

    // Single large random graph. Per-variant threads=1 baseline lands
    // around 20-30 s; two variants together stay under budget.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_8M_50M", random_input(algo, 8'000'000, 50'000'000), has_some_st()}
        };
    }

} // namespace

#define ST_VARIANT_PERF(ClassName, scenario_label, algo_key)              \
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

ST_VARIANT_PERF(NdSTPerf,          "NdST",          "nd_st")
ST_VARIANT_PERF(IncrementalSTPerf, "IncrementalST", "incremental_st")
