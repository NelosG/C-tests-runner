#include <algorithm>
#include <climits>
#include <numeric>
#include <quick_sort.h>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <vector>

class EdgeCaseTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        auto d = std::make_shared<std::vector<int>>();

        return {
            Test{
                "empty_array",
                [d]() { d->clear(); },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if(d->empty()) return {true, ""};
                    return {false, "Empty array should remain empty"};
                }
            },
            Test{
                "single_element",
                [d]() { *d = {42}; },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if(d->size() == 1 && (*d)[0] == 42) return {true, ""};
                    return {false, "Single element changed"};
                }
            },
            Test{
                "two_elements_reversed",
                [d]() { *d = {5, 3}; },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if((*d)[0] == 3 && (*d)[1] == 5) return {true, ""};
                    return {false, "Expected {3, 5}"};
                }
            },
            Test{
                "already_sorted",
                [d]() {
                    d->resize(1000);
                    std::iota(d->begin(), d->end(), 0);
                },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d->begin(), d->end())) return {true, ""};
                    return {false, "Already sorted array was corrupted"};
                }
            },
            Test{
                "reverse_sorted",
                [d]() {
                    d->resize(1000);
                    std::iota(d->rbegin(), d->rend(), 0);
                },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d->begin(), d->end())) return {true, ""};
                    return {false, "Reverse sorted not handled"};
                }
            },
            Test{
                "all_duplicates",
                [d]() { d->assign(500, 7); },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    bool ok = std::all_of(d->begin(), d->end(), [](int x) { return x == 7; });
                    if(ok && d->size() == 500) return {true, ""};
                    return {false, "Duplicate elements changed"};
                }
            },
            Test{
                "negative_numbers",
                [d]() { *d = {-100, -1, -50, 0, -999, 10, -5}; },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d->begin(), d->end())) return {true, ""};
                    return {false, "Negative number sorting failed"};
                }
            },
            Test{
                "extreme_values",
                [d]() { *d = {INT_MAX, INT_MIN, 0, -1, 1, INT_MAX - 1, INT_MIN + 1}; },
                [d]() { parallel::qsort(*d); },
                [d]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d->begin(), d->end())) return {true, ""};
                    return {false, "Extreme value sorting failed"};
                }
            }
        };
    }

    public:
        std::string name() const override { return "Correctness.EdgeCases"; }
        ~EdgeCaseTest() override = default;
};

REGISTER_TEST(EdgeCaseTest)