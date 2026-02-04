#pragma once
#include <test_scenario_result.h>
#include <nlohmann/json.hpp>

class TestScenarioResultConverter {
    public:
        static TestScenarioResultConverter& instance() {
            static TestScenarioResultConverter inst;
            return inst;
        }
        nlohmann::json to_json(const TestScenarioResult& testScenarioResult) const;
};
