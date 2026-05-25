#include <st_test_common.h>

namespace {

    using st_common::fixed_input;
    using st_common::random_input;
    using st_common::check_spanning_forest;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"tree_already",  fixed_input(algo, 5, {{0,1},{1,2},{2,3},{3,4}}),       check_spanning_forest()},
            {"single_cycle",  fixed_input(algo, 4, {{0,1},{1,2},{2,3},{3,0}}),       check_spanning_forest()},
            {"two_components",fixed_input(algo, 6, {{0,1},{1,2},{3,4},{4,5},{3,5}}), check_spanning_forest()},
            {"complete_4",    fixed_input(algo, 4, {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}}), check_spanning_forest()},
            {"random_100",    random_input(algo, 100,    300),                       check_spanning_forest()},
            {"random_1k",     random_input(algo, 1000,   3000),                      check_spanning_forest()},
            {"random_10k",    random_input(algo, 10'000, 30'000),                    check_spanning_forest()}
        };
    }

} // namespace

#define ST_VARIANT_SCENARIO(ClassName, scenario_label, algo_key)          \
    class ClassName final : public TestScenarioExtension {                \
        public:                                                           \
            std::vector<Test> get_tests() const override {                \
                return tests_for(algo_key);                               \
            }                                                             \
            std::string name() const override {                           \
                return "Correctness." scenario_label;                     \
            }                                                             \
            ScenarioType scenario_type() const override {                 \
                return ScenarioType::CORRECTNESS;                         \
            }                                                             \
    };                                                                    \
    REGISTER_TEST(ClassName)

ST_VARIANT_SCENARIO(NdSTCorrectness,          "NdST",          "nd_st")
ST_VARIANT_SCENARIO(IncrementalSTCorrectness, "IncrementalST", "incremental_st")
