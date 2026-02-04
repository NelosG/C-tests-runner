#include <test_result_converter.h>
#include <test_scenario_result_converter.h>

nlohmann::json TestScenarioResultConverter::to_json(const TestScenarioResult& testScenarioResult) const {
    nlohmann::json json;

    json["name"] = testScenarioResult.name;
    auto tests = nlohmann::json::array();

    const auto& converter = TestResultConverter::instance();

    for(const auto& test : testScenarioResult.results) {
        tests.push_back(converter.to_json(test));
    }
    json["tests"] = tests;
    return json;
}
