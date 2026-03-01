#pragma once

/**
 * @file resource_git.h
 * @brief Git-backed resource provider with persistent cache.
 *
 * Clones or fetches git repositories into a local cache directory.
 * Each unique (url, branch) pair maps to a deterministic cache subdirectory.
 * A background thread periodically removes entries that exceed cacheTtlSeconds.
 *
 * Config (resource-git.json):
 * {
 *   "cacheDir": "cache/git",
 *   "cacheTtlSeconds": 604800,
 *   "cleanupIntervalSeconds": 3600
 * }
 *
 * Descriptor: { "url": "...", "branch": "...", "token": "...", "_kind"?: "..." }
 * Auth: token is injected into the URL as https://oauth2:{token}@{rest} at clone/fetch time.
 * Token is NEVER written to disk (cache_meta.json stores only url + branch + last_pull_at).
 */

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <resource_context.h>
#include <resource_provider.h>
#include <string>
#include <thread>

class GitResourceProvider : public ResourceProvider {
    public:
        explicit GitResourceProvider(const ResourceContext& ctx);
        ~GitResourceProvider() override;

        std::string name() const override { return "git"; }

        /// Descriptor: { url, branch?, token?, _kind? }
        std::filesystem::path resolve(const nlohmann::json& descriptor) override;

        bool validateConfig(const nlohmann::json& config, std::string& error) override;

        void start() override;
        void stop() override;

    private:
        /// Compute a deterministic, human-readable cache directory name.
        static std::string makeCacheKey(const std::string& url, const std::string& branch);

        /// Inject token into URL: https://oauth2:{token}@{rest-of-url}
        static std::string injectToken(const std::string& url, const std::string& token);

        /// Clone repository to dest. Returns true on success.
        bool cloneRepo(
            const std::string& auth_url,
            const std::string& branch,
            const std::filesystem::path& dest
        ) const;

        /// Fetch latest commit in existing clone. Returns true on success.
        bool fetchRepo(
            const std::string& auth_url,
            const std::string& branch,
            const std::filesystem::path& dest
        ) const;

        /// Write .cache_meta.json (no token stored).
        static void writeCacheMeta(
            const std::filesystem::path& dir,
            const std::string& url,
            const std::string& branch
        );

        /// Background cleanup loop.
        void cleanupLoop();

        std::string cache_dir_;
        int cache_ttl_seconds_ = 604800; // 7 days
        int cleanup_interval_sec_ = 3600;   // 1 hour

        std::map<std::string, std::mutex> key_mutexes_; // per-key mutex for concurrent clones
        std::mutex key_mutexes_map_mutex_;               // protects key_mutexes_ map itself

        std::thread cleanup_thread_;
        std::atomic<bool> stop_{false};
        bool started_{false};
};