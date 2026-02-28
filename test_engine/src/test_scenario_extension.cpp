#include <chrono>
#include <new>
#include <test_scenario_extension.h>

TestResult TestScenarioExtension::runTest(const Test& test) {
    // Phase 1: Setup (NOT timed) — generate test data
    test.setup();

    // Phase 2: Execute (TIMED) — run student solution
    const auto start = std::chrono::high_resolution_clock::now();
    try {
        test.execute();
    } catch (const std::bad_alloc&) {
        // Memory limit exceeded — mark test as failed, keep timing
        const auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        return {test.name, false, "Memory limit exceeded", time_ms};
    }
    const auto end = std::chrono::high_resolution_clock::now();

    double time_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    // Phase 3: Verify (NOT timed) — check results
    auto [passed, message] = test.verify();

    return {
        test.name,
        passed,
        message,
        time_ms
    };
}

TestScenarioResult TestScenarioExtension::run() const {
    const auto tests = getTests();
    auto results = std::vector<TestResult>();
    results.reserve(tests.size());
    for(const auto& test : tests) {
        results.emplace_back(runTest(test));
    }
    return {
        name(),
        results
    };
}