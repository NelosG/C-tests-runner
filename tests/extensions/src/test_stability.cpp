#include <algorithm>
#include <map>
#include <quick_sort.h>
#include <random>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <vector>

class StabilityTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        auto original = std::make_shared<std::vector<int>>();
        auto data = std::make_shared<std::vector<int>>();
        auto ref = std::make_shared<std::vector<int>>();
        auto failed = std::make_shared<int>(-1);

        auto data2 = std::make_shared<std::vector<int>>();
        auto before = std::make_shared<std::map<int, int>>();

        auto d10k = std::make_shared<std::vector<int>>();
        auto d100k = std::make_shared<std::vector<int>>();

        return {
            Test{
                "repeated_sort_deterministic",
                // Setup: generate data and reference sort
                [original, ref]() {
                    std::mt19937 gen(12345);
                    std::uniform_int_distribution<int> dist(-10000, 10000);
                    original->resize(5000);
                    for(auto& x : *original) x = dist(gen);
                    *ref = *original;
                    parallel::qsort(*ref);
                },
                // Execute: sort the same data 5 more times
                [original, data, failed]() {
                    for(int i = 0; i < 5; ++i) {
                        *data = *original;
                        parallel::qsort(*data);
                    }
                },
                // Verify: all sorts produce same result
                [data, ref, failed]() -> std::pair<bool, std::string> {
                    if(*data != *ref)
                        return {false, "Repeated sort produced different result"};
                    return {true, ""};
                }
            },
            Test{
                "preserves_all_elements",
                [data2, before]() {
                    std::mt19937 gen(99999);
                    std::uniform_int_distribution<int> dist(0, 500);
                    data2->resize(10000);
                    for(auto& x : *data2) x = dist(gen);
                    before->clear();
                    for(int x : *data2) (*before)[x]++;
                },
                [data2]() { parallel::qsort(*data2); },
                [data2, before]() -> std::pair<bool, std::string> {
                    std::map<int, int> after;
                    for(int x : *data2) after[x]++;
                    if(*before != after)
                        return {false, "Element frequencies changed"};
                    if(!std::is_sorted(data2->begin(), data2->end()))
                        return {false, "Array not sorted"};
                    return {true, ""};
                }
            },
            Test{
                "random_10k",
                [d10k]() {
                    std::mt19937 gen(42);
                    d10k->resize(10000);
                    for(auto& x : *d10k) x = static_cast<int>(gen());
                },
                [d10k]() { parallel::qsort(*d10k); },
                [d10k]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d10k->begin(), d10k->end())) return {true, ""};
                    return {false, "10K random not sorted"};
                }
            },
            Test{
                "random_100k",
                [d100k]() {
                    std::mt19937 gen(777);
                    d100k->resize(100000);
                    for(auto& x : *d100k) x = static_cast<int>(gen());
                },
                [d100k]() { parallel::qsort(*d100k); },
                [d100k]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d100k->begin(), d100k->end())) return {true, ""};
                    return {false, "100K random not sorted"};
                }
            }
        };
    }

    public:
        std::string name() const override { return "Correctness.Stability"; }
        ~StabilityTest() override = default;
};

REGISTER_TEST(StabilityTest)