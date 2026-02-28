#pragma once

/**
 * @file adapter_status.h
 * @brief Adapter lifecycle status enum.
 */

#include <string>

/// Adapter lifecycle status.
enum class adapter_status {
    available,  ///< DLL found, not loaded
    running,    ///< Loaded and started
    started,    ///< Just loaded (response status)
    stopped,    ///< Just unloaded (response status)
    failed      ///< Load/unload failed
};

inline std::string to_string(adapter_status s) {
    switch(s) {
        case adapter_status::available: return "available";
        case adapter_status::running: return "running";
        case adapter_status::started: return "started";
        case adapter_status::stopped: return "stopped";
        case adapter_status::failed: return "failed";
    }
    return "available";
}