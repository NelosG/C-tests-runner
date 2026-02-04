#include <omp.h>
#include <test_engine.h>

std::vector<TestScenarioResult> TestEngine::executeCorrectness() const {
    omp_set_dynamic(0);
    omp_set_num_threads(omp_threads);

    const auto& tests = TestRegistry::instance().all();
    std::vector<TestScenarioResult> results;
    results.reserve(tests.size());

    for(size_t i = 0; i < tests.size(); ++i) {
        auto result = tests[i]->run();
        results.emplace_back(result);
    }
    return results;
}

std::vector<TestScenarioResult> TestEngine::executePerformance() const {
    omp_set_dynamic(0);
    omp_set_num_threads(omp_threads);

    const auto& tests = TestRegistry::instance().all();
    std::vector<TestScenarioResult> results;
    results.reserve(tests.size());

    for(size_t i = 0; i < tests.size(); ++i) {
        auto result = tests[i]->run();
        results.emplace_back(result);
    }
    return results;
}
