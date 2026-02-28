#pragma once

/**
 * @file resource_provider.h
 * @brief Abstract interface for resource provider plugins.
 *
 * A ResourceProvider resolves a JSON descriptor into a local filesystem path.
 * Implementations (git, local) are loaded as DLLs by ResourceManager.
 *
 * Lifecycle: create_provider() -> validateConfig() -> start() -> resolve() ... -> stop() -> destroy_provider()
 */

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

class ResourceProvider {
    public:
        virtual ~ResourceProvider() = default;

        /// Canonical provider name (e.g. "git", "local").
        virtual std::string name() const = 0;

        /**
     * @brief Resolve a source descriptor to a local filesystem path.
     *
     * Descriptor fields depend on provider type:
     *   - git:   { url, branch, token?, _kind? }
     *   - local: { path, _kind? }
     *
     * "_kind" is an optional internal field injected by TestRunnerService
     * ("solution" or "test") to select the appropriate base directory in
     * the local provider.
     *
     * @throws std::runtime_error on resolution failure.
     */
        virtual std::filesystem::path resolve(const nlohmann::json& descriptor) = 0;

        /**
     * @brief Validate provider config before starting.
     *
     * Called after create_provider() but before start(). If validation fails,
     * the provider is destroyed without starting.
     *
     * @param config  The provider's JSON config.
     * @param error   Populated with a human-readable error on failure.
     * @return true if config is valid, false otherwise.
     */
        virtual bool validateConfig(const nlohmann::json& config, std::string& error) = 0;

        /// Start background threads (e.g. cleanup loop). Called after validateConfig.
        virtual void start() {}

        /// Stop background threads. Called before destroy_provider.
        virtual void stop() {}
};