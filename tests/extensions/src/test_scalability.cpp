#include <algorithm>
#include <quick_sort.h>
#include <random>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <vector>

class ScalabilityTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        auto d1m = std::make_shared<std::vector<int>>();
        auto d5m = std::make_shared<std::vector<int>>();
        auto d20m = std::make_shared<std::vector<int>>();

        return {
            Test{
                "sort_1M",
                [d1m]() {
                    std::mt19937 gen(111);
                    d1m->resize(1'000'000);
                    for(auto& x : *d1m) x = static_cast<int>(gen());
                },
                [d1m]() { parallel::qsort(*d1m); },
                [d1m]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d1m->begin(), d1m->end())) return {true, ""};
                    return {false, "1M sort incorrect"};
                }
            },
            Test{
                "sort_5M",
                [d5m]() {
                    std::mt19937 gen(222);
                    d5m->resize(5'000'000);
                    for(auto& x : *d5m) x = static_cast<int>(gen());
                },
                [d5m]() { parallel::qsort(*d5m); },
                [d5m]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d5m->begin(), d5m->end())) return {true, ""};
                    return {false, "5M sort incorrect"};
                }
            },
            Test{
                "sort_20M",
                [d20m]() {
                    std::mt19937 gen(333);
                    d20m->resize(20'000'000);
                    for(auto& x : *d20m) x = static_cast<int>(gen());
                },
                [d20m]() { parallel::qsort(*d20m); },
                [d20m]() -> std::pair<bool, std::string> {
                    if(std::is_sorted(d20m->begin(), d20m->end())) return {true, ""};
                    return {false, "20M sort incorrect"};
                }
            }
        };
    }

    public:
        std::string name() const override { return "Performance.Scalability"; }
        ScenarioType scenario_type() const override { return ScenarioType::PERFORMANCE; }
        ~ScalabilityTest() override = default;
};

REGISTER_TEST(ScalabilityTest)