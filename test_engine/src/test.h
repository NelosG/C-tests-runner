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
    using SetupFn  = std::function<void(TestData& input)>;
    using VerifyFn = std::function<std::pair<bool, std::string>(
        const TestData& input, const TestData& output)>;

    Test(std::string name, SetupFn setup, VerifyFn verify)
        : name(std::move(name)), setup_(std::move(setup)), verify_(std::move(verify)) {}

    std::string name;

    void setup(TestData& input) const {
        if(setup_) setup_(input);
    }

    std::pair<bool, std::string> verify(
        const TestData& input, const TestData& output) const {
        return verify_(input, output);
    }

private:
    SetupFn  setup_;
    VerifyFn verify_;
};
