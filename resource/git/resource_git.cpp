#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <process_utils.h>
#include <register_resource_provider.h>
#include <resource_context.h>
#include <resource_git.h>
#include <stdexcept>
#include <thread>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// ============================================================================
// Utility: DJB2 hash + sanitized URL prefix → cache key
// ============================================================================

std::string GitResourceProvider::makeCacheKey(const std::string& url, const std::string& branch) {
    // DJB2 hash of url+branch
    uint64_t hash = 5381;
    for(char c : url + branch) hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);

    // Sanitized URL prefix (strip scheme, take up to 30 chars, replace non-alnum with _)
    std::string prefix = url;
    auto scheme_end = prefix.find("://");
    if(scheme_end != std::string::npos) prefix = prefix.substr(scheme_end + 3);
    if(prefix.size() > 30) prefix.resize(30);
    for(char& c : prefix) {
        if(!isalnum(static_cast<unsigned char>(c)) && c != '-') c = '_';
    }

    return prefix + "_" + std::to_string(hash);
}

// ============================================================================
// Utility: inject OAuth2 token into HTTPS URL
// ============================================================================

std::string GitResourceProvider::injectToken(const std::string& url, const std::string& token) {
    if(token.empty()) return url;
    // Inject as https://oauth2:{token}@{rest}
    auto pos = url.find("://");
    if(pos == std::string::npos) return url;
    return url.substr(0, pos + 3) + "oauth2:" + token + "@" + url.substr(pos + 3);
}

// ============================================================================
// Construction / Destruction
// ============================================================================

GitResourceProvider::GitResourceProvider(const ResourceContext& ctx) {
    cache_dir_ = ctx.config.value("cacheDir", "cache/git");
    cache_ttl_seconds_ = ctx.config.value("cacheTtlSeconds", 604800);
    cleanup_interval_sec_ = ctx.config.value("cleanupIntervalSeconds", 3600);
}

GitResourceProvider::~GitResourceProvider() {
    // Safety net: stop background thread if still running
    if(started_) {
        stop();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool GitResourceProvider::validateConfig(const nlohmann::json& config, std::string& error) {
    std::string cache_dir = config.value("cacheDir", "cache/git");
    fs::path cache_path(cache_dir);
    fs::path parent = cache_path.parent_path();

    // If cacheDir itself exists as a directory, that's fine
    if(fs::is_directory(cache_path)) return true;

    // Otherwise verify parent exists so we can create the dir
    if(!parent.empty() && !fs::is_directory(parent)) {
        // Try creating parent
        std::error_code ec;
        fs::create_directories(parent, ec);
        if(ec) {
            error = "cacheDir parent does not exist and cannot be created: " + parent.string();
            return false;
        }
    }
    return true;
}

void GitResourceProvider::start() {
    // Ensure cache directory exists
    fs::create_directories(cache_dir_);
    started_ = true;
    stop_ = false;
    cleanup_thread_ = std::thread(&GitResourceProvider::cleanupLoop, this);
    std::cout << "[GitProvider] Started (cacheDir=" << cache_dir_
        << ", ttl=" << cache_ttl_seconds_ << "s)\n";
}

void GitResourceProvider::stop() {
    if(!started_) return;
    stop_ = true;
    if(cleanup_thread_.joinable()) cleanup_thread_.join();
    started_ = false;
    std::cout << "[GitProvider] Stopped\n";
}

// ============================================================================
// Resolve: clone or fetch
// ============================================================================

std::filesystem::path GitResourceProvider::resolve(const nlohmann::json& descriptor) {
    std::string url = descriptor.value("url", "");
    std::string branch = descriptor.value("branch", "");
    std::string token = descriptor.value("token", "");

    if(url.empty()) {
        throw std::runtime_error("[GitProvider] Descriptor missing 'url' field");
    }

    std::string cache_key = makeCacheKey(url, branch);
    fs::path cache_path = fs::path(cache_dir_) / cache_key;

    // Get or create per-key mutex
    std::mutex* key_mutex;
    {
        std::lock_guard map_lock(key_mutexes_map_mutex_);
        key_mutex = &key_mutexes_[cache_key];
    }

    std::lock_guard key_lock(*key_mutex);

    std::string auth_url = injectToken(url, token);

    if(!fs::is_directory(cache_path)) {
        std::cout << "[GitProvider] Cloning " << url << " (branch=" << branch << ")...\n";
        if(!cloneRepo(auth_url, branch, cache_path)) {
            throw std::runtime_error("[GitProvider] Clone failed for: " + url);
        }
    } else {
        std::cout << "[GitProvider] Fetching " << url << " (branch=" << branch << ")...\n";
        if(!fetchRepo(auth_url, branch, cache_path)) {
            throw std::runtime_error("[GitProvider] Fetch failed for: " + url);
        }
    }

    writeCacheMeta(cache_path, url, branch); // url without token
    return cache_path;
}

// ============================================================================
// Git operations
// ============================================================================

bool GitResourceProvider::cloneRepo(
    const std::string& auth_url,
    const std::string& branch,
    const fs::path& dest
) const {
    std::string cmd = "git clone --depth 1";
    if(!branch.empty()) {
        cmd += " --branch \"" + branch + "\"";
    }
    cmd += " \"" + auth_url + "\" \"" + dest.string() + "\" 2>&1";
    auto result = runCommand(cmd);
    if(result.failed()) {
        std::cerr << "[GitProvider] Clone failed: " << result.output << "\n";
        return false;
    }
    return true;
}

bool GitResourceProvider::fetchRepo(
    const std::string& auth_url,
    const std::string& branch,
    const fs::path& dest
) const {
    // Update remote URL (with token)
    std::string set_url_cmd = "git -C \"" + dest.string() + "\" remote set-url origin \""
        + auth_url + "\" 2>&1";
    auto set_result = runCommand(set_url_cmd);
    if(set_result.failed()) {
        std::cerr << "[GitProvider] remote set-url failed: " << set_result.output << "\n";
        return false;
    }

    // Fetch
    std::string fetch_cmd = "git -C \"" + dest.string() + "\" fetch --depth 1 origin";
    if(!branch.empty()) {
        fetch_cmd += " \"" + branch + "\"";
    }
    fetch_cmd += " 2>&1";
    auto fetch_result = runCommand(fetch_cmd);
    if(fetch_result.failed()) {
        std::cerr << "[GitProvider] Fetch failed: " << fetch_result.output << "\n";
        return false;
    }

    // Reset to FETCH_HEAD
    std::string reset_cmd = "git -C \"" + dest.string() + "\" reset --hard FETCH_HEAD 2>&1";
    auto reset_result = runCommand(reset_cmd);
    if(reset_result.failed()) {
        std::cerr << "[GitProvider] Reset failed: " << reset_result.output << "\n";
        return false;
    }

    return true;
}

// ============================================================================
// Cache metadata
// ============================================================================

void GitResourceProvider::writeCacheMeta(
    const fs::path& dir,
    const std::string& url,
    const std::string& branch
) {
    // Get current time as ISO8601 string (reuse time_utils approach)
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_t));

    nlohmann::json meta = {
        {"url", url},              // no token
        {"branch", branch},
        {"last_pull_at", buf}
    };

    std::ofstream f(dir / ".cache_meta.json");
    if(f.is_open()) {
        f << meta.dump(2) << "\n";
    }
}

// ============================================================================
// Background cleanup
// ============================================================================

void GitResourceProvider::cleanupLoop() {
    while(!stop_) {
        // Sleep in small increments to check stop_ flag
        for(int i = 0; i < cleanup_interval_sec_ * 10 && !stop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if(stop_) break;

        std::cout << "[GitProvider] Running cache cleanup...\n";

        std::error_code ec;
        if(!fs::is_directory(cache_dir_)) continue;

        auto now = std::chrono::system_clock::now();

        for(const auto& entry : fs::directory_iterator(cache_dir_, ec)) {
            if(ec) break;
            if(!entry.is_directory()) continue;

            fs::path meta_path = entry.path() / ".cache_meta.json";
            if(!fs::exists(meta_path)) continue;

            try {
                std::ifstream f(meta_path);
                auto meta = nlohmann::json::parse(f);
                std::string last_pull_at = meta.value("last_pull_at", "");
                if(last_pull_at.empty()) continue;

                // Parse ISO8601 time (strptime not portable on Windows)
                struct tm tm{};
                int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
                if(std::sscanf(
                    last_pull_at.c_str(),
                    "%d-%d-%dT%d:%d:%dZ",
                    &y,
                    &mo,
                    &d,
                    &h,
                    &mi,
                    &s
                ) != 6)
                    continue;
                tm.tm_year = y - 1900;
                tm.tm_mon = mo - 1;
                tm.tm_mday = d;
                tm.tm_hour = h;
                tm.tm_min = mi;
                tm.tm_sec = s;
                tm.tm_isdst = -1;
                auto entry_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));

                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry_time).count();
                if(age > cache_ttl_seconds_) {
                    std::cout << "[GitProvider] Removing expired cache: "
                        << entry.path().filename().string() << "\n";
                    fs::remove_all(entry.path(), ec);
                }
            } catch(const std::exception& e) {
                std::cerr << "[GitProvider] Cleanup error for " << entry.path() << ": " << e.what() << "\n";
            }
        }
    }
}

REGISTER_RESOURCE_PROVIDER(GitResourceProvider, "git")