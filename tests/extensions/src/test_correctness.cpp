#include <algorithm>
#include <quick_sort.h>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <vector>

class CorrectnessTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        // Shared data via shared_ptr so lambdas can capture by value (copyable)
        auto a1 = std::make_shared<std::vector<int>>();
        auto a2 = std::make_shared<std::vector<int>>();

        return {
            Test{
                "sorted_small",
                [a1]() { *a1 = {1, 5, 5, 6, 2, 43, 6, -1, -7}; },
                [a1]() { parallel::qsort(*a1); },
                [a1]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(a1->begin(), a1->end()))
                        return {true, ""};
                    return {false, "Array not sorted"};
                }
            },
            Test{
                "sorted_with_message",
                [a2]() { *a2 = {1, 5, 5, 6, 2, 43, 6, -1, -7}; },
                [a2]() { parallel::qsort(*a2); },
                [a2]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(a2->begin(), a2->end()))
                        return {true, "correct"};
                    return {false, "INCORRECT"};
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