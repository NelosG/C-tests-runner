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
 *   create_adapter()  — constructs the adapter from an AdapterContext
 *   destroy_adapter() — deletes the adapter
 *
 * The adapter class must have a constructor with signature:
 *   ClassName(TestRunnerService&, const ManagementAPI*, const AdapterContext& ctx)
 */

#include <adapter_api.h>
#include <adapter_context.h>
#include <iostream>
#include <test_runner_service.h>

#define REGISTER_ADAPTER(ClassName, AdapterName)                              \
extern "C" {                                                                  \
                                                                              \
ADAPTER_API const char* adapter_name() {                                      \
    return AdapterName;                                                       \
}                                                                             \
                                                                              \
ADAPTER_API TransportAdapter* create_adapter(TestRunnerService* runner,       \
                                              const ManagementAPI* mgmt,      \
                                              const AdapterContext* ctx) {    \
    if (!runner || !ctx) return nullptr;                                      \
    try {                                                                     \
        return new ClassName(*runner, mgmt, *ctx);                            \
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
