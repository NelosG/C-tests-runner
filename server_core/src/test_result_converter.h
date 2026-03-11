#pragma once

/**
 * @file test_result_converter.h
 * @brief Converts TestResult to JSON.
 */

#include <test_result.h>
#include <nlohmann/json.hpp>


namespace TestResultConverter {

    /**
 * @brief Convert a TestResult to JSON.
 * @param testResult The result to serialize.
 * @return JSON object with name, passed, message (if non-empty), time_ms.
 */
    nlohmann::json to_json(const TestResult& testResult);

} // namespace TestResultConverter