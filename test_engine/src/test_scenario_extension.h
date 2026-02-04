#pragma once
#include <string>
#include <test.h>
#include <test_result.h>
#include <test_scenario_result.h>

class TestScenarioExtension {
    virtual std::vector<Test> getTests() const = 0;

    static TestResult runTest(const Test& test);

    public:
        virtual ~TestScenarioExtension() = default;

        TestScenarioResult run() const;

        virtual std::string name() const = 0;
};
