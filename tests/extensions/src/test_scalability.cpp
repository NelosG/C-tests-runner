#include <test_builder.h>

class ScalabilityTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {
                    "sort_1M",
                    setup::random_array<long long>("array", 1'000'000, 111),
                    verify::same_elements("array", "result")
                },
                {
                    "sort_5M",
                    setup::random_array<long long>("array", 5'000'000, 222),
                    verify::same_elements("array", "result")
                },
                {
                    "sort_20M",
                    setup::random_array<long long>("array", 20'000'000, 333),
                    verify::same_elements("array", "result")
                },
            };
        }

        std::string name() const override { return "Performance.Scalability"; }
        ScenarioType scenario_type() const override { return ScenarioType::PERFORMANCE; }
};

REGISTER_TEST(ScalabilityTest)
