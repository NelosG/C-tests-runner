#pragma once

/**
 * @file register_adapter.h
 * @brief One-line macro for adapter DLL registration.
 *
 * Usage (at the end of an adapter .cpp file):
 *   REGISTER_ADAPTER(HttpAdapter, "http")
 *
 * The macro generates extern "C" factory functions expected by AdapterManager:
 *   adapter_name()    — returns the canonical name string
 *   create_adapter()  — parses config JSON and constructs the adapter
 *   destroy_adapter() — deletes the adapter
 *
 * The adapter class must have a constructor with signature:
 *   ClassName(TestRunnerService&, const ManagementAPI*, const nlohmann::json& config)
 */

#include <adapter_api.h>
#include <iostream>
#include <test_runner_service.h>
#include <nlohmann/json.hpp>

#define REGISTER_ADAPTER(ClassName, AdapterName)                              \
extern "C" {                                                                  \
                                                                              \
ADAPTER_API const char* adapter_name() {                                      \
    return AdapterName;                                                       \
}                                                                             \
                                                                              \
ADAPTER_API TransportAdapter* create_adapter(TestRunnerService* runner,       \
                                              const ManagementAPI* mgmt,      \
                                              const char* config_json) {      \
    if (!runner) return nullptr;                                              \
    nlohmann::json config;                                                    \
    try { config = nlohmann::json::parse(config_json); }                      \
    catch (const std::exception& e) {                                         \
        std::cerr << "[" AdapterName "] config parse error: "                 \
                  << e.what() << "\n";                                        \
        return nullptr;                                                       \
    }                                                                         \
    try {                                                                     \
        return new ClassName(*runner, mgmt, config);                          \
    } catch (const std::exception& e) {                                       \
        std::cerr << "[" AdapterName "] create_adapter failed: "              \
                  << e.what() << "\n";                                        \
        return nullptr;                                                       \
    }                                                                         \
}                                                                             \
                                                                              \
ADAPTER_API void destroy_adapter(TransportAdapter* adapter) {                 \
    delete adapter;                                                           \
}                                                                             \
                                                                              \
}
