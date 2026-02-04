#include <algorithm>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <quick_sort.h>

class PerfTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        return {
            {
                "performance",
                []() -> std::pair<bool, std::string> {
                    constexpr auto size = 10000000;
                    auto a = std::vector<int>(size);

                    for(int i = 0; i < size; ++i) {
                        a[i] = rand();
                    }

                    parallel::qsort(a);

                    if(std::is_sorted(a.begin(), a.end())) {
                        return {true, ""};
                    }

                    return {true, "INCORRECT"};
                }
            }
        };
    }

    public:
        std::string name() const override {
            return "Performance.Basic";
        }

        ~PerfTest() override = default;
};

REGISTER_TEST(PerfTest)
