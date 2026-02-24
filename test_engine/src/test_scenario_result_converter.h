#pragma once

/**
 * @file test_scenario_result_converter.h
 * @brief Converts TestScenarioResult collections to grouped JSON.
 */

#include <test_scenario_result.h>
#include <vector>
#include <nlohmann/json.hpp>


namespace TestScenarioResultConverter {

    /**
 * @brief Convert a flat results vector into grouped JSON scenarios array.
 *
 * Results from engine.execute() come in order:
 *   [scenario0@tc0, scenario1@tc0, ..., scenario0@tc1, scenario1@tc1, ...]
 *
 * This method groups them as: scenario -> test -> runs (per thread count).
 *
 * @param results       Flat vector from TestEngine::execute().
 * @param thread_counts Thread counts used (same order as execute() call).
 * @param perf_mode     If true, compute and attach performance metrics.
 * @param numa_node     NUMA node to tag results with (-1 = none).
 * @return JSON array of grouped scenarios.
 */
    nlohmann::json to_grouped_json(
        const std::vector<TestScenarioResult>& results,
        const std::vector<int>& thread_counts,
        bool perf_mode = false,
        int numa_node = -1
    );

    /**
 * @brief Convert a single TestScenarioResult to flat JSON.
 * @param testScenarioResult The scenario result to serialize.
 * @return JSON object with name, threads, tests[], and optional metrics.
 */
    nlohmann::json to_json(const TestScenarioResult& testScenarioResult);

} // namespace TestScenarioResultConverter