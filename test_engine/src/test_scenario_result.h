#pragma once

/**
 * @file test_scenario_result.h
 * @brief Aggregated results for one test scenario at a specific thread count.
 */

#include <string>
#include <test_result.h>
#include <utility>
#include <vector>

/**
 * @brief Groups individual TestResult entries under a scenario name.
 *
 * Per-test monitoring stats (parallel_regions, work_ns, etc.) are stored
 * in each TestResult, not here.
 */
struct TestScenarioResult {
    std::string name;
    std::vector<TestResult> results;
    int threads = 1;
    int numa_node = -1;

    TestScenarioResult(std::string name, std::vector<TestResult> results, int threads = 1)
        : name(std::move(name)),
          results(std::move(results)),
          threads(threads) {}
};