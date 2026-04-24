#include <test_builder.h>

class CorrectnessTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {
                    "sorted_small",
                    setup::array<long long>("array", {1, 5, 5, 6, 2, 43, 6, -1, -7}),
                    verify::same_elements("array", "result")
                },
                {
                    "sorted_with_message",
                    setup::array<long long>("array", {1, 5, 5, 6, 2, 43, 6, -1, -7}),
                    verify::same_elements("array", "result")
                },
            };
        }

        std::string name() const override { return "Correctness.Basic"; }
};

REGISTER_TEST(CorrectnessTest)
