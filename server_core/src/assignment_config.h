#pragma once

/**
 * @file assignment_config.h
 * @brief Teacher's assignment settings parsed from <test_dir>/config.json.
 *
 * Pure parser - no engine state, no side effects beyond reading the file.
 * The orchestrator is expected to validate `allowed_frameworks` is non-empty.
 *
 * Resource limit fields (threads, memory_limit_mb, wall_time_sec, cpu_time_sec,
 * max_processes) are optional - if absent the pipeline keeps using the
 * orchestrator's request value, or the server-level default if the request
 * did not specify one. Priority: request > test-config.json > server default.
 */

#include <filesystem>
#include <optional>
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

    // Per-assignment resource caps. nullopt = "no opinion, fall through to the
    // server default (or the orchestrator's request value if it set one).
    std::optional<int> threads;                        ///< default thread count for tests
    std::optional<long long> memory_limit_mb;          ///< per-test memory cap (MB)
    std::optional<int> wall_time_sec;                  ///< wall-clock budget per test (sec)
    std::optional<int> cpu_time_sec;                   ///< cpu-time budget per test (sec)
    std::optional<int> max_processes;                  ///< sandbox process cap
    /// Number of untimed warmup iterations of RUNNER_EXECUTE before the
    /// timed iteration. 0 = no warmup (default, safe for in-place algos).
    /// 1+ = pre-fault parlay::sequence allocations / page caches so the
    /// measured body sees a warm process; required to recover ~14x-like
    /// speedup on memory-bound algos (histogram, integerSort, ...).
    /// Only safe when the wrapper does NOT mutate its input arguments.
    std::optional<int> warmup_iterations;
};


namespace assignment_config {
    /// Read <test_dir>/config.json. Missing file or parse errors yield a
    /// config with empty allowed_frameworks (caller reports the error).
    AssignmentConfig load(const std::filesystem::path& test_dir);
}
