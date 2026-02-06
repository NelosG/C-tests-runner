#pragma once

/**
 * @file test_registry.h
 * @brief Registry for dynamically loaded test scenarios.
 *
 * Supports both singleton usage (CLI tools) and per-job instances
 * (concurrent server jobs). When a thread-local active instance is set
 * via setActiveInstance(), REGISTER_TEST() calls route there instead
 * of the global singleton.
 */

#include <memory>
#include <test_scenario_extension.h>
#include <vector>

class TestRegistry {
    public:
        /**
     * @brief Access the active registry instance.
     *
     * Returns the thread-local active instance if set,
     * otherwise the global singleton.
     */
        static TestRegistry& instance();

        /**
     * @brief Set the thread-local active instance.
     *
     * Call before loading plugins so REGISTER_TEST() static initializers
     * register into a per-job registry instead of the global singleton.
     */
        static void setActiveInstance(TestRegistry* registry);

        /** @brief Clear the thread-local active instance (revert to global). */
        static void clearActiveInstance();

        bool register_test(std::unique_ptr<TestScenarioExtension> test);

        const std::vector<std::unique_ptr<TestScenarioExtension>>& all() const;

        void clear();

        size_t size() const;

        TestRegistry() = default;
        ~TestRegistry() = default;

        TestRegistry(const TestRegistry&) = delete;
        TestRegistry& operator=(const TestRegistry&) = delete;

    private:
        std::vector<std::unique_ptr<TestScenarioExtension>> tests_;
};