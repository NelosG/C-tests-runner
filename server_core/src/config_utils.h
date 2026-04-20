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
    inline nlohmann::json read_json_file(const std::filesystem::path& path) {
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
    inline std::string get_env(const char* name, const std::string& default_value = "") {
        const char* val = std::getenv(name);
        return (val && val[0]) ? std::string(val) : default_value;
    }

    /// Typed server config (config/server.json).
    /// All `default*` fields are per-job fall-backs - overridden by the value
    /// in the request JSON when the orchestrator sends one.
    struct ServerConfig {
        std::vector<std::string> defaultAdapters;
        std::vector<std::string> defaultResourceProviders;
        std::optional<int> correctnessWorkers;
        std::optional<std::string> nodeId;
        long long defaultMemoryLimitMb = 1024;     ///< Per-job memory limit (MB), 0 = unlimited.
        int defaultThreads = 4;                    ///< Default thread count when request has no `threads`.
        int defaultWallTimeSec = 60;               ///< Default wall-clock limit when request has no `wallTimeSec`.
        int defaultCpuTimeSec = 30;                ///< Default cpu-time limit when request has no `cpuTimeSec`.
        int sandboxProcessMultiplier =
            2;          ///< max_processes = threads * multiplier (unless `maxProcesses` is in request).

        static ServerConfig load(const std::filesystem::path& path) {
            auto json = read_json_file(path);
            ServerConfig cfg;
            if(json.contains("defaultAdapters") && json["defaultAdapters"].is_array()) {
                for(const auto& a : json["defaultAdapters"]) {
                    if(a.is_string()) cfg.defaultAdapters.push_back(a.get<std::string>());
                }
            }
            if(json.contains("defaultResourceProviders") && json["defaultResourceProviders"].is_array()) {
                for(const auto& p : json["defaultResourceProviders"]) {
                    if(p.is_string()) cfg.defaultResourceProviders.push_back(p.get<std::string>());
                }
            }
            if(json.contains("correctnessWorkers") && json["correctnessWorkers"].is_number()) {
                cfg.correctnessWorkers = json["correctnessWorkers"].get<int>();
            }
            if(json.contains("nodeId") && json["nodeId"].is_string()) {
                cfg.nodeId = json["nodeId"].get<std::string>();
            }
            if(json.contains("defaultMemoryLimitMb") && json["defaultMemoryLimitMb"].is_number()) {
                cfg.defaultMemoryLimitMb = json["defaultMemoryLimitMb"].get<long long>();
            }
            if(json.contains("defaultThreads") && json["defaultThreads"].is_number_integer()) {
                cfg.defaultThreads = json["defaultThreads"].get<int>();
            }
            if(json.contains("defaultWallTimeSec") && json["defaultWallTimeSec"].is_number_integer()) {
                cfg.defaultWallTimeSec = json["defaultWallTimeSec"].get<int>();
            }
            if(json.contains("defaultCpuTimeSec") && json["defaultCpuTimeSec"].is_number_integer()) {
                cfg.defaultCpuTimeSec = json["defaultCpuTimeSec"].get<int>();
            }
            if(json.contains("sandbox") && json["sandbox"].is_object()) {
                const auto& sb = json["sandbox"];
                if(sb.contains("processMultiplier") && sb["processMultiplier"].is_number_integer()) {
                    cfg.sandboxProcessMultiplier = sb["processMultiplier"].get<int>();
                }
            }
            return cfg;
        }
    };

} // namespace config
