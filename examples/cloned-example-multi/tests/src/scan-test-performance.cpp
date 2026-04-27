#include <test_builder.h>


namespace {

    auto verify_inclusive_scan = [](
        const TestData& in,
        const TestData& out
    )
        -> std::pair<bool, std::string> {
        auto input = in.read_array<long long>("array");
        auto output = out.read_array<long long>("result");
        if(input.size() != output.size())
            return {false, "Size mismatch"};
        long long sum = 0;
        for(std::size_t i = 0; i < input.size(); ++i) {
            sum += input[i];
            if(sum != output[i])
                return {false, "Mismatch at index " + std::to_string(i)};
        }
        return {true, ""};
    };

} // namespace

class ScanPerformanceTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                {"perf_1e7", setup::random_array<long long>("array", 10000000), verify_inclusive_scan},
                {"perf_5e7", setup::random_array<long long>("array", 50000000), verify_inclusive_scan},
            };
        }

        std::string name() const override { return "Performance.Scan"; }
        ScenarioType scenario_type() const override { return ScenarioType::PERFORMANCE; }
};

REGISTER_TEST(ScanPerformanceTest)
