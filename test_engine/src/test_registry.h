#pragma once
#include <memory>
#include <test_scenario_extension.h>
#include <vector>

class TestRegistry {
    public:
        static TestRegistry& instance() {
            static TestRegistry inst;
            return inst;
        }

        bool register_test(std::unique_ptr<TestScenarioExtension> test);
        bool register_tests(const std::vector<std::unique_ptr<TestScenarioExtension>>& tests);

        const std::vector<std::unique_ptr<TestScenarioExtension>>& all() const;

        ~TestRegistry() = default;

    private:
        std::vector<std::unique_ptr<TestScenarioExtension>> tests;
};
