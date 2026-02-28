#pragma once

/**
 * @file resource_provider_status.h
 * @brief Resource provider lifecycle status enum.
 */

#include <string>

/// Resource provider lifecycle status.
enum class resource_provider_status {
    available,  ///< DLL found, not loaded
    running,    ///< Loaded and started
    started,    ///< Just loaded (response status)
    stopped,    ///< Just unloaded (response status)
    failed      ///< Load/unload failed
};

inline std::string to_string(resource_provider_status s) {
    switch(s) {
        case resource_provider_status::available: return "available";
        case resource_provider_status::running: return "running";
        case resource_provider_status::started: return "started";
        case resource_provider_status::stopped: return "stopped";
        case resource_provider_status::failed: return "failed";
    }
    return "available";
}