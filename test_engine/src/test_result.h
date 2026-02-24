#pragma once

/**
 * @file test_result.h
 * @brief Data class for a single test outcome with monitoring stats.
 */

#include <string>
#include <utility>

/**
 * @brief Stores the result of executing a single Test.
 *
 * Includes pass/fail, wall-clock time, and per-test parallel monitoring
 * statistics (construct counts, work/span).
 */
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    double time_ms = 0.0;

    // Per-test monitoring stats (from par::monitor, reset between tests)
    int parallel_regions = 0;
    int tasks_created = 0;
    int max_threads_used = 0;
    int single_regions = 0;
    int taskwaits = 0;
    int barriers = 0;
    int criticals = 0;
    int for_loops = 0;
    int atomics = 0;
    int sections = 0;
    int masters = 0;
    int ordered = 0;
    int taskgroups = 0;
    int simd_constructs = 0;
    int cancels = 0;
    int flushes = 0;
    int taskyields = 0;
    long long work_ns = 0;
    long long span_ns = 0;

    TestResult(
        std::string name,
        const bool passed,
        std::string message,
        const double time_ms = 0.0
    ) : name(std::move(name)), passed(passed), message(std::move(message)), time_ms(time_ms) {}
};