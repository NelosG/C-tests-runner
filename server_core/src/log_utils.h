#pragma once

/**
 * @file log_utils.h
 * @brief Minimal logging utilities: timestamp prefix for server console output.
 */

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>


namespace log_utils {

    /// Returns current local time as "HH:MM:SS.mmm"
    inline std::string ts() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count() % 1000;
        struct tm tm_buf{};
        #ifdef _WIN32
        localtime_s(&tm_buf, &time);
        #else
        localtime_r(&time, &tm_buf);
        #endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms;
        return ss.str();
    }

} // namespace log_utils

/// Log to stdout: [HH:MM:SS.mmm] [Component] message
#define LOG(component) std::cout << "[" << log_utils::ts() << "] [" << component << "] "

/// Log to stderr: [HH:MM:SS.mmm] [Component] message
#define LOG_ERR(component) std::cerr << "[" << log_utils::ts() << "] [" << component << "] "
