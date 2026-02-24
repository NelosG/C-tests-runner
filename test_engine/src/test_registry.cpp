#include <iostream>
#include <test_registry.h>

static thread_local TestRegistry* tl_active_registry = nullptr;

TestRegistry& TestRegistry::instance() {
    if(tl_active_registry) {
        return *tl_active_registry;
    }
    static TestRegistry global;
    return global;
}

void TestRegistry::setActiveInstance(TestRegistry* registry) {
    tl_active_registry = registry;
}

void TestRegistry::clearActiveInstance() {
    tl_active_registry = nullptr;
}

bool TestRegistry::register_test(std::unique_ptr<TestScenarioExtension> test) {
    if(!test) return false;

    const auto& name = test->name();
    for(const auto& existing : tests_) {
        if(existing->name() == name) {
            std::cerr << "[TestRegistry] Warning: duplicate test '" << name << "' — skipped\n";
            return false;
        }
    }

    tests_.push_back(std::move(test));
    return true;
}

const std::vector<std::unique_ptr<TestScenarioExtension>>& TestRegistry::all() const {
    return tests_;
}

void TestRegistry::clear() {
    tests_.clear();
}

size_t TestRegistry::size() const {
    return tests_.size();
}