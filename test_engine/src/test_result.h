#pragma once
#include <string>
#include <utility>
class TestResult {
    public:

        TestResult(
            std::string name,
            const bool passed,
            std::string message,
            const double time_ms = 0.0
        ) : name(std::move(name)), passed(passed), message(std::move(message)), time_ms(time_ms) {}

        std::string name;
        bool passed;
        std::string message;
        double time_ms = 0.0;

        ~TestResult() = default;
};
