#pragma once

/**
 * @file time_utils.h
 * @brief Time formatting utilities (header-only).
 */

#include <chrono>
#include <ctime>
#include <string>

inline std::string nowISO8601() {
    const auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}
