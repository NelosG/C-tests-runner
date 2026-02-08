#pragma once

/**
 * @file config_utils.h
 * @brief JSON config reading and environment variable helpers.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace config {

/// Read a JSON file; returns empty object on failure.
inline nlohmann::json readJsonFile(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return {};
    std::ifstream f(path);
    if(!f.is_open()) return {};
    try {
        return nlohmann::json::parse(f);
    } catch(const std::exception& e) {
        std::cerr << "[Config] Invalid JSON in " << path.filename() << ": " << e.what() << "\n";
        return {};
    }
}

/// Get an environment variable with a default fallback.
inline std::string getEnv(const char* name, const std::string& default_value = "") {
    const char* val = std::getenv(name);
    return (val && val[0]) ? std::string(val) : default_value;
}

/// Typed server config (config/server.json).
struct ServerConfig {
    std::vector<std::string> defaultAdapters;
    std::optional<int> correctnessWorkers;

    static ServerConfig load(const std::filesystem::path& path) {
        auto json = readJsonFile(path);
        ServerConfig cfg;
        if(json.contains("defaultAdapters")) {
            for(const auto& a : json["defaultAdapters"]) {
                cfg.defaultAdapters.push_back(a.get<std::string>());
            }
        }
        if(json.contains("correctnessWorkers")) {
            cfg.correctnessWorkers = json["correctnessWorkers"].get<int>();
        }
        return cfg;
    }
};

} // namespace config
