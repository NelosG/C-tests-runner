#include <mis_test_common.h>

namespace {

    using mis_common::fixed_input;
    using mis_common::random_input;
    using mis_common::check_mis;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single_vertex",   fixed_input(algo, 1, {}),                                 check_mis()},
            {"isolated_5",      fixed_input(algo, 5, {}),                                 check_mis()},
            {"single_edge",     fixed_input(algo, 2, {{0,1}}),                            check_mis()},
            {"triangle",        fixed_input(algo, 3, {{0,1},{1,2},{0,2}}),                check_mis()},
            {"path_5",          fixed_input(algo, 5, {{0,1},{1,2},{2,3},{3,4}}),          check_mis()},
            {"star_5",          fixed_input(algo, 5, {{0,1},{0,2},{0,3},{0,4}}),          check_mis()},
            {"cycle_6",         fixed_input(algo, 6, {{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}}), check_mis()},
            {"random_100_300",  random_input(algo, 100,  300),                            check_mis()},
            {"random_1k_3k",    random_input(algo, 1000, 3000),                           check_mis()},
            {"random_10k_30k",  random_input(algo, 10'000, 30'000),                       check_mis()}
        };
    }

} // namespace

#define MIS_VARIANT_SCENARIO(ClassName, scenario_label, algo_key)         \
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

MIS_VARIANT_SCENARIO(NdMISCorrectness,          "NdMIS",          "nd_mis")
MIS_VARIANT_SCENARIO(IncrementalMISCorrectness, "IncrementalMIS", "incremental_mis")
