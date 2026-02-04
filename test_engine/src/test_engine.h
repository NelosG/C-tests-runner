#pragma once

#include <test_registry.h>

class TestEngine {
    public:
        TestEngine() {
            if(const char* env = std::getenv("OMP_THREADS")) {
                omp_threads = std::atoi(env);
            }


        }
        ~TestEngine() = default;

        std::vector<TestScenarioResult> executeCorrectness() const;
        std::vector<TestScenarioResult> executePerformance() const;

        int omp_threads = 1;
};

