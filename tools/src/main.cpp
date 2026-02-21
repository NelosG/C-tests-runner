/**
 * @file main.cpp
 * @brief CLI entry point for local test execution (no HTTP, no queue).
 *
 * Usage: ./run <plugins_dir> [--mode correctness|performance|all] [--threads N]
 *
 * Loads pre-built plugins from a directory, runs tests, and prints JSON
 * results to stdout. Designed for local development and debugging on Windows.
 */

#include <filesystem>
#include <iostream>
#include <plugin_loader.h>
#include <string>
#include <test_engine.h>
#include <test_registry.h>
#include <test_scenario_result_converter.h>
#include <thread_counts.h>
#include <vector>
#include <nlohmann/json.hpp>
#include <par/monitor.h>

namespace fs = std::filesystem;

static std::string getPluginsDirectory(const std::string& exec_path) {
    const fs::path exec_dir = fs::path(exec_path).parent_path();
    const fs::path plugins_dir = exec_dir / "plugins";
    return plugins_dir.string();
}

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [plugins_dir] [--mode correctness|performance|all] [--threads N]\n";
}

static nlohmann::json runMode(
    const std::string& mode_name,
    par::Mode monitor_mode,
    ScenarioType type,
    int threads,
    TestRegistry& registry
) {
    auto ctx = par::monitor::createContext();
    ctx->mode = monitor_mode;
    ctx->max_threads = threads;
    par::monitor::activateContext(ctx.get());

    TestEngine engine;
    auto thread_counts = ThreadCounts::get(mode_name, threads);
    auto results = engine.execute(thread_counts, registry, *ctx, type);

    par::monitor::activateContext(nullptr);

    return TestScenarioResultConverter::to_grouped_json(
        results,
        thread_counts,
        type == ScenarioType::PERFORMANCE
    );
}

int main(int argc, char** argv) {
    std::string plugins_dir;
    std::string mode = "all";
    int threads = 32;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if(arg == "--threads" && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        } else if(arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if(plugins_dir.empty() && arg[0] != '-') {
            plugins_dir = arg;
        }
    }

    if(plugins_dir.empty()) {
        plugins_dir = getPluginsDirectory(argv[0]);
    }

    std::cout << "=== C++ Test Runner (CLI) ===\n";
    std::cout << "Plugins dir: " << plugins_dir << "\n";
    std::cout << "Mode: " << mode << ", Threads: " << threads << "\n";

    PluginLoader loader;
    size_t loaded = loader.loadPluginsFromDirectory(plugins_dir);

    if(loaded == 0) {
        std::cerr << "Warning: No plugins loaded! Make sure .so/.dll files are in: " << plugins_dir << "\n";
    }

    std::cout << "\nRegistered test scenarios: " << TestRegistry::instance().size() << "\n";
    for(const auto& test : TestRegistry::instance().all()) {
        std::cout << "  - " << test->name() << "\n";
    }
    std::cout << "\n";

    nlohmann::json report;
    report["mode"] = mode;

    if(mode == "correctness" || mode == "all") {
        report["correctness"] = runMode(
            "correctness",
            par::Mode::STRESS,
            ScenarioType::CORRECTNESS,
            threads,
            TestRegistry::instance()
        );
    }
    if(mode == "performance" || mode == "all") {
        report["performance"] = runMode(
            "performance",
            par::Mode::MONITOR,
            ScenarioType::PERFORMANCE,
            threads,
            TestRegistry::instance()
        );
    }

    std::cout << report.dump(4) << '\n';

    loader.unloadAll();
    return 0;
}