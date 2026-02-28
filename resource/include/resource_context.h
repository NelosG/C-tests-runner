#pragma once

/**
 * @file resource_context.h
 * @brief Context passed to resource provider constructors.
 */

#include <nlohmann/json.hpp>

struct ResourceContext {
    nlohmann::json config;  ///< Provider-specific JSON config (parsed from resource-{name}.json)
};