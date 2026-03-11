#pragma once

/**
 * @file test_engine.h
 * @brief Core test execution engine with OpenMP thread control.
 */

#include <test_registry.h>
#include <test_scenario_extension.h>
#include <vector>
#include <par/monitor.h>

class TestEngine {
    public:
        TestEngine() = default;
        ~TestEngine() = default;

        /**
     * @brief Execute scenarios matching the given type at each thread count.
     * @param thread_counts List of thread counts to iterate over.
     * @param registry Registry containing all test scenarios.
     * @param ctx Monitor context for this execution.
     * @param type_filter Only run scenarios of this type.
     * @return One TestScenarioResult per (matching scenario, thread_count) pair.
     */
        std::vector<TestScenarioResult> execute(
            const std::vector<int>& thread_counts,
            const TestRegistry& registry,
            par::MonitorContext& ctx,
            ScenarioType type_filter
        );
};