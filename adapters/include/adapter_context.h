#pragma once

/**
 * @file adapter_context.h
 * @brief Context passed to adapter constructors — node-level config beyond adapter JSON.
 */

#include <string>
#include <nlohmann/json.hpp>

struct AdapterContext {
    nlohmann::json config;       ///< Adapter-specific JSON (parsed from http.json / rabbit.json)
    std::string node_id;         ///< Global node identifier for this server instance
    std::string adapter_name;    ///< Canonical adapter name (from DLL filename, e.g. "http", "rabbit")
};