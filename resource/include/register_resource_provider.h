#pragma once

/**
 * @file register_resource_provider.h
 * @brief One-line macro for resource provider DLL registration.
 *
 * Usage (at the end of a resource provider .cpp file):
 *   REGISTER_RESOURCE_PROVIDER(GitResourceProvider, "git")
 *
 * The macro generates extern "C" factory functions expected by ResourceManager:
 *   provider_name()    — returns the canonical name string
 *   create_provider()  — constructs the provider from a ResourceContext
 *   destroy_provider() — deletes the provider
 *
 * The provider class must have a constructor with signature:
 *   ClassName(const ResourceContext& ctx)
 */

#include <resource_context.h>
#include <resource_provider.h>
#include <iostream>

// --- Export macro ---
#ifdef _WIN32
#ifdef RESOURCE_PROVIDER_EXPORTS
#define RESOURCE_API __declspec(dllexport)
#else
#define RESOURCE_API __declspec(dllimport)
#endif
#else
#define RESOURCE_API __attribute__((visibility("default")))
#endif

#define REGISTER_RESOURCE_PROVIDER(ClassName, ProviderName)                          \
extern "C" {                                                                          \
                                                                                      \
RESOURCE_API const char* provider_name() {                                            \
    return ProviderName;                                                              \
}                                                                                     \
                                                                                      \
RESOURCE_API ResourceProvider* create_provider(const ResourceContext* ctx) {         \
    if (!ctx) return nullptr;                                                         \
    try {                                                                             \
        return new ClassName(*ctx);                                                   \
    } catch (const std::exception& e) {                                               \
        std::cerr << "[" ProviderName "] create_provider failed: "                   \
                  << e.what() << "\n";                                               \
        return nullptr;                                                               \
    }                                                                                 \
}                                                                                     \
                                                                                      \
RESOURCE_API void destroy_provider(ResourceProvider* provider) {                     \
    delete provider;                                                                  \
}                                                                                     \
                                                                                      \
}
