#pragma once

/**
 * @file test.h
 * @brief Single test case with setup/verify phases.
 *
 * The two-phase design separates data preparation from result verification.
 * Execution is handled by the runner process in a sandbox.
 *
 * Setup populates a TestData (input). The engine serializes it to input.bin
 * inside the sandbox bind-mount, runs the runner, then deserializes
 * output.bin into another TestData and hands both to verify().
 */

#include <functional>
#include <string>
#include <test_data.h>
#include <utility>

class Test {
    public:
        using SetupFn = std::function<void(TestData& input)>;
        // verify gets const refs - TestData::read_* are non-consuming, so verify
        // lambdas can re-read keys idempotently. Peak-memory savings happen on
        // the runner side via runner::read_* (parse + erase).
        using VerifyFn = std::function<std::pair<bool, std::string>(
            const TestData& input,
            const TestData& output
        )>;

        Test(std::string name, SetupFn setup, VerifyFn verify)
            : name(std::move(name)), setup_(std::move(setup)), verify_(std::move(verify)) {}

        std::string name;

        /// Optional: path to a ready-made TLV blob. When set, the engine
        /// copies the file straight into the sandbox as input.bin and
        /// skips setup_(). Used for "external" inputs (e.g. converted
        /// from pbbs's test-data generators).
        std::string raw_input_path;

        /// Optional: path to an expected TLV output blob. When set the
        /// engine byte-compares the runner's output.bin to this file
        /// instead of calling verify_(). Empty -> verify_() is used.
        std::string expected_output_path;

        bool has_raw_input() const { return !raw_input_path.empty(); }
        bool has_expected_output() const { return !expected_output_path.empty(); }

        void setup(TestData& input) const {
            if(setup_) setup_(input);
        }

        std::pair<bool, std::string> verify(
            const TestData& input,
            const TestData& output
        ) const {
            return verify_ ? verify_(input, output)
                           : std::pair<bool, std::string>{true, std::string{}};
        }

    private:
        SetupFn setup_;
        VerifyFn verify_;
};
