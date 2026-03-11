#pragma once

/**
 * @file api_types.h
 * @brief Shared enum types for API contracts.
 */

#include <stdexcept>
#include <string>

/// Test execution mode.
enum class test_mode {
    correctness,
    performance,
    all
};

inline std::string to_string(test_mode m) {
    switch(m) {
        case test_mode::correctness: return "correctness";
        case test_mode::performance: return "performance";
        case test_mode::all: return "all";
    }
    return "correctness";
}

inline test_mode test_mode_from_string(const std::string& s) {
    if(s == "correctness") return test_mode::correctness;
    if(s == "performance") return test_mode::performance;
    if(s == "all") return test_mode::all;
    throw std::invalid_argument("Invalid mode: '" + s + "'. Must be 'correctness', 'performance', or 'all'");
}

inline bool is_valid_test_mode(const std::string& s) {
    return s == "correctness" || s == "performance" || s == "all";
}

/// Queue lane (derived from test_mode).
inline std::string lane_for_mode(test_mode m) {
    return (m == test_mode::performance || m == test_mode::all)
        ? to_string(test_mode::performance)
        : to_string(test_mode::correctness);
}

inline std::string lane_for_mode(const std::string& mode) {
    return (mode == to_string(test_mode::performance) || mode == to_string(test_mode::all))
        ? to_string(test_mode::performance)
        : to_string(test_mode::correctness);
}

/// Job lifecycle status.
enum class job_status {
    queued,
    building,
    running,
    completed,
    failed,
    cancelled
};

inline std::string to_string(job_status s) {
    switch(s) {
        case job_status::queued: return "queued";
        case job_status::building: return "building";
        case job_status::running: return "running";
        case job_status::completed: return "completed";
        case job_status::failed: return "failed";
        case job_status::cancelled: return "cancelled";
    }
    return "queued";
}

inline job_status job_status_from_string(const std::string& s) {
    if(s == "queued") return job_status::queued;
    if(s == "building") return job_status::building;
    if(s == "running") return job_status::running;
    if(s == "completed") return job_status::completed;
    if(s == "failed") return job_status::failed;
    if(s == "cancelled") return job_status::cancelled;
    throw std::invalid_argument("Invalid job status: '" + s + "'");
}

/// Queue overall status (busy/idle).
enum class queue_status { busy, idle };

inline std::string to_string(queue_status s) {
    switch(s) {
        case queue_status::busy: return "busy";
        case queue_status::idle: return "idle";
    }
    return "idle";
}

/// Response status for adapter replies.
enum class response_status { ok, error, rejected };

inline std::string to_string(response_status s) {
    switch(s) {
        case response_status::ok: return "ok";
        case response_status::error: return "error";
        case response_status::rejected: return "rejected";
    }
    return "error";
}