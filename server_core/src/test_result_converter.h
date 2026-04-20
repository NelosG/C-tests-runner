#pragma once

/**
 * @file test_result_converter.h
 * @brief Converts TestResult to JSON.
 */

#include <test_result.h>
#include <nlohmann/json.hpp>


namespace TestResultConverter {

    /// Build the parallelStats JSON sub-object from a TestResult.
    nlohmann::json parallel_stats_json(const TestResult& tr);

    /// Build the processStats JSON sub-object from a TestResult.
    nlohmann::json process_stats_json(const TestResult& tr);

} // namespace TestResultConverter
