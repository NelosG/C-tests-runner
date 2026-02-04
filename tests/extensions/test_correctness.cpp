#include <algorithm>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <quick_sort.h>

class CorrectnessTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        return {
            {
                "correctness1",
                []() -> std::pair<bool, std::string> {
                    auto a = std::vector{1, 5, 5, 6, 2, 43, 6, -1, -7};

                    parallel::qsort(a);

                    if(std::is_sorted(a.begin(), a.end())) {
                        return {true, ""};
                    }

                    return {true, "INCORRECT"};
                }
            },

            {
                "correctness2",
                []() -> std::pair<bool, std::string> {
                    auto a = std::vector{1, 5, 5, 6, 2, 43, 6, -1, -7};

                    parallel::qsort(a);

                    if(std::is_sorted(a.begin(), a.end())) {
                        return {true, "correct"};
                    }

                    return {true, "INCORRECT"};
                }
            }
        };
    }

    public:
        std::string name() const override {
            return "Correctness.Basic";
        }

        ~CorrectnessTest() override = default;
};

REGISTER_TEST(CorrectnessTest)
