#include <chrono>
#include <test_registry.h>
#include <test_scenario_extension.h>

TestResult TestScenarioExtension::runTest(const Test& test) {
    const auto start = std::chrono::high_resolution_clock::now();

    auto [result, message] = test();

    const auto end = std::chrono::high_resolution_clock::now();
    auto time_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    return {
        test.name,
        result,
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
