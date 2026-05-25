#include <isort_test_common.h>
#include <limits>

namespace {

    using isort_common::fixed_input;
    using isort_common::random_input;
    using isort_common::check_sort;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single",          fixed_input(algo, {42}, 32),                       check_sort()},
            {"sorted_small",    fixed_input(algo, {5, 3, 1, 4, 2}, 8),            check_sort()},
            {"already_sorted",  fixed_input(algo, {1, 2, 3, 4, 5}, 8),            check_sort()},
            {"reverse_sorted",  fixed_input(algo, {9, 7, 5, 3, 1}, 8),            check_sort()},
            {"all_duplicates",  fixed_input(algo, {7, 7, 7, 7, 7}, 8),            check_sort()},
            {"extreme_values",  fixed_input(algo, {
                                    std::numeric_limits<std::uint32_t>::max(),
                                    0, 1, 100, 50}, 32),                           check_sort()},
            {"random_10k_24b",  random_input(algo, 10'000,    24),                 check_sort()},
            {"random_100k_24b", random_input(algo, 100'000,   24),                 check_sort()},
            {"random_1M_24b",   random_input(algo, 1'000'000, 24),                 check_sort()}
        };
    }

} // namespace

class ParallelRadixSortCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_radix_sort");
        }
        std::string name() const override {
            return "Correctness.ParallelRadixSort";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelRadixSortCorrectness)
