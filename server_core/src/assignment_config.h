#pragma once

/**
 * @file assignment_config.h
 * @brief Teacher's assignment settings parsed from <test_dir>/config.json.
 *
 * Pure parser - no engine state, no side effects beyond reading the file.
 * The orchestrator is expected to validate `allowed_frameworks` is non-empty.
 */

#include <filesystem>
#include <string>
#include <vector>

struct AssignmentConfig {
    std::string name;
    std::string mode = "correctness";                  ///< "correctness" | "performance" | "all"
    std::vector<std::string>
    allowed_frameworks;       ///< subset of {"openmp", "parlay", "cilk"}; empty == invalid config
    std::vector<std::string>
    allowed_packages;         ///< whitelist for CMakeValidator; defaulted from allowed_frameworks if not set
    std::string correctness_mode = "stress";           ///< "stress" | "monitor"
};


namespace assignment_config {
    /// Read <test_dir>/config.json. Missing file or parse errors yield a
    /// config with empty allowed_frameworks (caller reports the error).
    AssignmentConfig load(const std::filesystem::path& test_dir);
}
