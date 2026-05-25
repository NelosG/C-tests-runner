#include <matching_test_common.h>

namespace {

    using matching_common::fixed_input;
    using matching_common::random_input;
    using matching_common::check_matching;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single_edge",  fixed_input(algo, 2, {{0,1}}),                          check_matching()},
            {"triangle",     fixed_input(algo, 3, {{0,1},{1,2},{0,2}}),              check_matching()},
            {"path_5",       fixed_input(algo, 5, {{0,1},{1,2},{2,3},{3,4}}),        check_matching()},
            {"star_5",       fixed_input(algo, 5, {{0,1},{0,2},{0,3},{0,4}}),        check_matching()},
            {"two_paths",    fixed_input(algo, 6, {{0,1},{1,2},{3,4},{4,5}}),        check_matching()},
            {"random_100",   random_input(algo, 100,  300),                           check_matching()},
            {"random_1k",    random_input(algo, 1000, 3000),                          check_matching()},
            {"random_10k",   random_input(algo, 10'000, 30'000),                      check_matching()}
        };
    }

} // namespace

#define MATCH_VARIANT_SCENARIO(ClassName, scenario_label, algo_key)       \
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

MATCH_VARIANT_SCENARIO(NdMatchingCorrectness,          "NdMatching",          "nd_matching")
MATCH_VARIANT_SCENARIO(IncrementalMatchingCorrectness, "IncrementalMatching", "incremental_matching")
