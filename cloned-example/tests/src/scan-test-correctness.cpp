#include <random>
#include <register_test.h>
#include <scan.h>
#include <test_scenario_extension.h>
#include <vector>

class ScanCorrectnessTest final : public TestScenarioExtension {
    std::vector<Test> getTests() const override {
        constexpr int array_size = 1e8;

        auto vec = std::make_shared<std::vector<long long>>(array_size);
        auto result = std::make_shared<std::vector<long long>>(array_size);

        return {
            Test{
                "1e8",
                [vec]() {
                    std::mt19937_64 gen(42);
                    std::generate(vec->begin(), vec->end(), gen);
                },
                [vec, result]() {
                    parallel::scan(*vec, *result);
                },
                [vec, result]() -> std::pair<bool, std::string> {
                    if(vec->size() != result->size()) {
                        return {false, "Wrong result size"};
                    }

                    long long counter = 0;
                    for(size_t i = 0; i < vec->size(); ++i) {
                        counter += (*vec)[i];
                        if(counter != (*result)[i]) {
                            return {false, "Scan failed at index " + std::to_string(i)};
                        }
                    }
                    return {true, ""};
                }
            }
        };
    }

    public:
        std::string name() const override { return "SCAN_CORRECTNESS.Basic"; }
};

REGISTER_TEST(ScanCorrectnessTest)