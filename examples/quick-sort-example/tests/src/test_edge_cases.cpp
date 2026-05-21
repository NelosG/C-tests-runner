#include <climits>
#include <test_builder.h>

class EdgeCaseTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {
                    "empty_array",
                    setup::array<long long>("array", {}),
                    verify::equals<long long>("result", {})
                },
                {
                    "single_element",
                    setup::array<long long>("array", {42}),
                    verify::equals<long long>("result", {42})
                },
                {
                    "two_elements_reversed",
                    setup::array<long long>("array", {5, 3}),
                    verify::same_elements("array", "result")
                },
                {
                    "already_sorted",
                    setup::random_array<long long>("array", 1000, 100),
                    verify::same_elements("array", "result")
                },
                {
                    "all_duplicates",
                    setup::array<long long>("array", std::vector<long long>(500, 7)),
                    verify::same_elements("array", "result")
                },
                {
                    "negative_numbers",
                    setup::array<long long>("array", {-100, -1, -50, 0, -999, 10, -5}),
                    verify::same_elements("array", "result")
                },
                {
                    "extreme_values",
                    setup::array<long long>("array", {INT_MAX, INT_MIN, 0, -1, 1, INT_MAX - 1, INT_MIN + 1}),
                    verify::same_elements("array", "result")
                },
            };
        }

        std::string name() const override { return "Correctness.EdgeCases"; }
};

REGISTER_TEST(EdgeCaseTest)
