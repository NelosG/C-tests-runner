#include <algorithm>
#include <random>
#include <test_builder.h>

class StabilityTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {
                    "repeated_sort_deterministic",
                    // Custom setup: write array + pre-sorted reference into the input map.
                    [](TestData& in) {
                        std::mt19937 gen(12345);
                        std::uniform_int_distribution<int> dist(-10000, 10000);
                        std::vector<long long> data(5000);
                        for(auto& x : data) x = dist(gen);
                        auto ref = data;
                        std::sort(ref.begin(), ref.end());
                        in.write_array<long long>("array", data);
                        in.write_array<long long>("reference", ref);
                    },
                    verify::matches_reference<long long>("reference", "result")
                },
                {
                    "preserves_elements",
                    setup::random_array<long long>("array", 10000, 99999),
                    verify::same_elements<long long>("array", "result")
                },
                {
                    "random_10k",
                    setup::random_array<long long>("array", 10000, 42),
                    verify::same_elements<long long>("array", "result")
                },
                {
                    "random_100k",
                    setup::random_array<long long>("array", 100000, 777),
                    verify::same_elements<long long>("array", "result")
                },
            };
        }

        std::string name() const override { return "Correctness.Stability"; }
};

REGISTER_TEST(StabilityTest)
