#include "assignment_config.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;


namespace {

    /// Default whitelist for CMakeValidator: union of canonical packages
    /// across the frameworks the teacher allowed.
    std::vector<std::string> default_allowed_packages(const std::vector<std::string>& frameworks) {
        std::vector<std::string> result;
        auto append_unique = [&](std::initializer_list<const char*> pkgs) {
            for(const auto* p : pkgs) {
                if(std::find(result.begin(), result.end(), p) == result.end())
                    result.emplace_back(p);
            }
        };
        for(const auto& fw : frameworks) {
            if(fw == "openmp") append_unique({"OpenMP", "parallel_lib"});
            else if(fw == "parlay") append_unique({"parlay", "parlaylib"});
            else if(fw == "cilk") append_unique({"Cilk"});
        }
        return result;
    }

} // namespace

AssignmentConfig assignment_config::load(const fs::path& test_dir) {
    AssignmentConfig cfg;
    fs::path path = test_dir / "config.json";
    if(!fs::exists(path)) return cfg;

    std::ifstream f(path);
    if(!f.is_open()) return cfg;

    try {
        auto j = nlohmann::json::parse(f);
        cfg.name = j.value("name", "");
        cfg.mode = j.value("mode", "correctness");
        cfg.correctness_mode = j.value("correctnessMode", "stress");
        if(j.contains("allowedFrameworks"))
            cfg.allowed_frameworks = j["allowedFrameworks"].get<std::vector<std::string>>();
        if(j.contains("allowedPackages"))
            cfg.allowed_packages = j["allowedPackages"].get<std::vector<std::string>>();

        // Optional resource caps. Skip non-integer values - we don't want a
        // typo'd field to crash assignment parsing for an otherwise-valid repo.
        auto read_int = [&](const char* key) -> std::optional<int> {
            if(j.contains(key) && j[key].is_number_integer()) return j[key].get<int>();
            return std::nullopt;
        };
        auto read_int64 = [&](const char* key) -> std::optional<long long> {
            if(j.contains(key) && j[key].is_number_integer()) return j[key].get<long long>();
            return std::nullopt;
        };
        cfg.threads          = read_int("threads");
        cfg.memory_limit_mb  = read_int64("memoryLimitMb");
        cfg.wall_time_sec    = read_int("wallTimeSec");
        cfg.cpu_time_sec     = read_int("cpuTimeSec");
        cfg.max_processes    = read_int("maxProcesses");
    } catch(const std::exception& e) {
        std::cerr << "[AssignmentConfig] Failed to parse " << path << ": " << e.what() << "\n";
    }

    if(cfg.allowed_packages.empty()) {
        cfg.allowed_packages = default_allowed_packages(cfg.allowed_frameworks);
    }
    return cfg;
}
