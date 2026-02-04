#pragma once
#include <string>
#include <test_result.h>
#include <utility>
#include <vector>

class TestScenarioResult {
    public:
        TestScenarioResult(std::string name, std::vector<TestResult> results)
            : name(std::move(name)),
              results(std::move(results)) {}

        std::string name;
        std::vector<TestResult> results;

        ~TestScenarioResult() = default;
};
