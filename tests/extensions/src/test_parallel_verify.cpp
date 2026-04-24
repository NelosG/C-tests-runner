#include <random>
#include <test_builder.h>

class ParallelVerifyTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {
                    "uses_parallel_constructs",
                    setup::random_array<long long>("array", 50000, 55555),
                    verify::same_elements<long long>("array", "result")
                },
                {
                    "stress_test_races",
                    [](TestData& in) {
                        std::mt19937 gen(77777);
                        std::vector<long long> data(10000);
                        for(auto& x : data) x = static_cast<long long>(gen());
                        auto ref = data;
                        std::sort(ref.begin(), ref.end());
                        in.write_array<long long>("array", data);
                        in.write_array<long long>("reference", ref);
                    },
                    verify::matches_reference<long long>("reference", "result")
                },
                {
                    "uses_multiple_threads",
                    setup::random_array<long long>("array", 100000, 88888),
                    verify::same_elements<long long>("array", "result")
                },
            };
        }

        std::string name() const override { return "Correctness.ParallelVerify"; }
};

REGISTER_TEST(ParallelVerifyTest)
