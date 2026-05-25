// Correctness scenarios for each of the four required sort variants.
// All scenarios share the same body (fixed-input edge cases + random
// inputs at increasing sizes) - only the dispatch key differs.

#include <limits>
#include <sort_test_common.h>

namespace {

    using sort_common::fixed_input;
    using sort_common::random_input;
    using sort_common::sorted_permutation_of_input;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"sorted_small",
             fixed_input(algo, {5, 3, 1, 4, 2}),
             sorted_permutation_of_input()},
            {"already_sorted",
             fixed_input(algo, {1, 2, 3, 4, 5}),
             sorted_permutation_of_input()},
            {"reverse_sorted",
             fixed_input(algo, {9, 7, 5, 3, 1}),
             sorted_permutation_of_input()},
            {"all_duplicates",
             fixed_input(algo, {7, 7, 7, 7, 7}),
             sorted_permutation_of_input()},
            {"empty",
             fixed_input(algo, {}),
             sorted_permutation_of_input()},
            {"single_element",
             fixed_input(algo, {42}),
             sorted_permutation_of_input()},
            {"extreme_values",
             fixed_input(algo, {
                 std::numeric_limits<int>::max(),
                 std::numeric_limits<int>::min(),
                 0, -1, 1
             }),
             sorted_permutation_of_input()},
            {"random_10k",
             random_input(algo, 10'000),
             sorted_permutation_of_input()},
            {"random_100k",
             random_input(algo, 100'000),
             sorted_permutation_of_input()},
            {"random_1M",
             random_input(algo, 1'000'000),
             sorted_permutation_of_input()}
        };
    }

} // namespace

#define SORT_VARIANT_SCENARIO(ClassName, scenario_label, algo_key)        \
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

SORT_VARIANT_SCENARIO(ParlaySampleSortCorrectness,    "ParlaySampleSort",    "parlay_sample_sort")
SORT_VARIANT_SCENARIO(QuickSortCorrectness,           "QuickSort",           "quick_sort")
SORT_VARIANT_SCENARIO(MergeSortCorrectness,           "MergeSort",           "merge_sort")
SORT_VARIANT_SCENARIO(StableSampleSortCorrectness,    "StableSampleSort",    "stable_sample_sort")
