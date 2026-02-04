#pragma once
#include <test_result.h>
#include <nlohmann/json.hpp>

class TestResultConverter {
    public:
        static TestResultConverter& instance() {
            static TestResultConverter inst;
            return inst;
        }
        nlohmann::json to_json(const TestResult& testResult) const;
};
