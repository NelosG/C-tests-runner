#pragma once

#include <string>

class TestRegistry;

/// Loads JSON test scenario files from {test_dir}/cases/*.json
/// and registers them as virtual TestScenarioExtension in the registry.
class JsonScenarioLoader {
    public:
        static void load(const std::string& test_dir, TestRegistry& registry);
};
