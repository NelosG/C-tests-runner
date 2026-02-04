#include <test_result_converter.h>

nlohmann::json TestResultConverter::to_json(const TestResult& testResult) const {
    nlohmann::json json;

    json["name"] = testResult.name;
    json["passed"] = testResult.passed;
    if(!testResult.message.empty()) {
        json["message"] = testResult.message;
    }
    json["time_ms"] = testResult.time_ms;
    return json;
}
