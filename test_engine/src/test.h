#pragma once

/**
 * @file test.h
 * @brief Single test case with setup/execute/verify phases.
 *
 * The three-phase design ensures that only the student's solution execution
 * is timed, not test data generation or result verification.
 */

#include <functional>
#include <string>
#include <utility>

/**
 * @brief A single test case with separate setup, execute, and verify phases.
 *
 * - **setup()**: Generate test input data (not timed).
 * - **execute()**: Run the student's solution (timed).
 * - **verify()**: Check the result (not timed). Returns (passed, message).
 *
 * This separation ensures timing accuracy: data generation and validation
 * overhead is excluded from the measurement.
 */
class Test {
    public:
        using SetupFn = std::function<void()>;
        using ExecuteFn = std::function<void()>;
        using VerifyFn = std::function<std::pair<bool, std::string>()>;

        /**
     * @brief Construct a three-phase test.
     * @param name     Human-readable test name.
     * @param setup    Data generation / preparation (not timed).
     * @param execute  Student code invocation (timed).
     * @param verify   Result validation (not timed). Returns (passed, message).
     */
        Test(std::string name, SetupFn setup, ExecuteFn execute, VerifyFn verify)
            : name(std::move(name))
              , setup_(std::move(setup))
              , execute_(std::move(execute))
              , verify_(std::move(verify)) {}

        std::string name; ///< Human-readable test identifier.

        /** @brief Run the setup phase (not timed). */
        void setup() const { if(setup_) setup_(); }

        /** @brief Run the execution phase (timed). */
        void execute() const { execute_(); }

        /** @brief Run the verification phase (not timed). */
        std::pair<bool, std::string> verify() const { return verify_(); }

    private:
        SetupFn setup_;
        ExecuteFn execute_;
        VerifyFn verify_;
};