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
 * Auth: token is injected into the URL as `oauth2:<token>@host` (GitLab git-smart-http
 * requires Basic Auth, not Bearer headers) and passed to git via
 * `-c url.<auth>.insteadOf=<plain>` for the lifetime of a single invocation.
 * `.git/config` only ever stores the plain (token-less) URL - the token never
 * touches the disk. cache_meta.json stores only `url` (without token) + `branch`
 * + `last_pull_at`.
 * Auth failures surface as plain errors because `GIT_TERMINAL_PROMPT=0` is set in
 * the parent process during `start()` - git children inherit it via popen.
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

        bool validate_config(const nlohmann::json& config, std::string& error) override;

        void start() override;
        void stop() override;

    private:
        /// Compute a deterministic, human-readable cache directory name.
        static std::string make_cache_key(const std::string& url, const std::string& branch);

        /// Clone repository to dest. The plain URL is what gets written to
        /// `.git/config`; the auth URL with `oauth2:<token>` is supplied
        /// per-invocation via `-c url.<auth>.insteadOf=<plain>`. Returns
        /// true on success.
        bool clone_repo(
            const std::string& url,
            const std::string& branch,
            const std::string& token,
            const std::filesystem::path& dest
        ) const;

        /// Fetch latest commit in existing clone. Same `-c insteadOf` trick
        /// as clone_repo - `.git/config` is never modified by this call.
        /// Returns true on success.
        bool fetch_repo(
            const std::string& url,
            const std::string& branch,
            const std::string& token,
            const std::filesystem::path& dest
        ) const;

        /// Write .cache_meta.json (no token stored).
        static void write_cache_meta(
            const std::filesystem::path& dir,
            const std::string& url,
            const std::string& branch
        );

        /// Background cleanup loop.
        void cleanup_loop();

        // The three config fields below are read by the background cleanup
        // thread without synchronisation. They are written ONLY by the
        // constructor (before start()), and must not be mutated while the
        // provider is running. ResourceProvider has no live-config update
        // hook today; if one is ever added, these need to become atomic or
        // be guarded by a dedicated mutex.
        std::string cache_dir_;
        int cache_ttl_seconds_ = 604800; // 7 days
        int cleanup_interval_sec_ = 3600;   // 1 hour

        std::map<std::string, std::mutex> key_mutexes_; // per-key mutex for concurrent clones
        std::mutex key_mutexes_map_mutex_;               // protects key_mutexes_ map itself

        std::thread cleanup_thread_;
        std::atomic<bool> stop_{false};
        std::atomic<bool> started_{false};
};
