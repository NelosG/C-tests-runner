#pragma once

/**
 * @file mock_utils.h
 * @brief Shared utilities for mock orchestrator tools (RabbitMQ and HTTP).
 */

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

/// Summarize test results from a "correctness" or "performance" JSON section.
inline std::string summarizeTests(const nlohmann::json& section) {
    if(!section.is_object() || !section.contains("scenarios")) return "";
    int passed = 0, total = 0;
    for(auto& scenario : section["scenarios"]) {
        for(auto& test : scenario.value("tests", nlohmann::json::array())) {
            total++;
            if(test.value("passed", false)) passed++;
        }
    }
    return std::to_string(passed) + "/" + std::to_string(total) + " passed";
}

struct RunArgs {
    std::string test_id;
    std::string test_dir;
    std::vector<std::string> solutions;
    std::string mode = "all";
    int threads = 4;
    long long memory_limit_mb = -1; ///< -1 means "not specified" (use server default)
};

inline bool parseRunArgs(const std::string& line, RunArgs& args) {
    std::istringstream iss(line);
    std::string token;
    std::vector<std::string> tokens;
    while(iss >> token) tokens.push_back(token);

    try {
        for(size_t i = 0; i < tokens.size(); ++i) {
            const auto& t = tokens[i];
            if(t == "--test-id" && i + 1 < tokens.size()) args.test_id = tokens[++i];
            else if(t == "--test-dir" && i + 1 < tokens.size()) args.test_dir = tokens[++i];
            else if(t == "--solution" && i + 1 < tokens.size()) args.solutions.push_back(tokens[++i]);
            else if(t == "--mode" && i + 1 < tokens.size()) args.mode = tokens[++i];
            else if(t == "--threads" && i + 1 < tokens.size()) args.threads = std::stoi(tokens[++i]);
            else if(t == "--memory" && i + 1 < tokens.size()) args.memory_limit_mb = std::stoll(tokens[++i]);
        }
    } catch(const std::exception& e) {
        std::cerr << "[Mock] Invalid argument: " << e.what() << "\n";
        return false;
    }

    if(args.test_id.empty() || args.test_dir.empty() || args.solutions.empty()) {
        std::cerr << "[Mock] Usage: run --test-id <id> --test-dir <dir> --solution <dir>"
            " [--solution <dir2>] [--mode all] [--threads 4] [--memory 1024]\n";
        return false;
    }
    return true;
}