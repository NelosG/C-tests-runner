#include <test_registry.h>

bool TestRegistry::register_test(std::unique_ptr<TestScenarioExtension> test) {
    tests.push_back(std::move(test));
    return true;
}

bool TestRegistry::register_tests(const std::vector<std::unique_ptr<TestScenarioExtension>>& tests) {
    for(auto& test : tests) {
        register_test(std::unique_ptr<TestScenarioExtension>(test.get()));
    }
    return true;
}


const std::vector<std::unique_ptr<TestScenarioExtension>>& TestRegistry::all() const {
    return tests;
}
