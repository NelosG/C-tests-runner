#pragma once

/**
 * @file test_scenario_extension.h
 * @brief Abstract base class for test scenario plugins.
 */

#include <string>
#include <test.h>

/**
 * @brief Type of test scenario - determines when it runs.
 */
enum class ScenarioType {
    CORRECTNESS,  ///< Runs with STRESS mode, multiple thread counts.
    PERFORMANCE   ///< Runs with MONITOR mode, {1, N} threads, exclusive CPU.
};

/**
 * @brief Abstract base that every test plugin must extend.
 *
 * Subclasses implement get_tests(), name(), and optionally scenario_type()
 * to declare whether the scenario is for correctness or performance testing.
 *
 * The engine filters scenarios by type and only runs matching ones.
 * Plugins register their subclass via the REGISTER_TEST() macro.
 */
class TestScenarioExtension {
    public:
        virtual ~TestScenarioExtension() = default;

        virtual std::vector<Test> get_tests() const = 0;

        virtual std::string name() const = 0;

        /**
     * @brief Declare the scenario type.
     * @return CORRECTNESS (default) or PERFORMANCE.
     *
     * Override in performance test plugins:
     * @code
     * ScenarioType scenario_type() const override { return ScenarioType::PERFORMANCE; }
     * @endcode
     */
        virtual ScenarioType scenario_type() const { return ScenarioType::CORRECTNESS; }

};
