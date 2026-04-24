#include <chrono>
#include <cstdlib>
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
// Utility: DJB2 hash + sanitized URL prefix -> cache key
// ============================================================================

std::string GitResourceProvider::make_cache_key(const std::string& url, const std::string& branch) {
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
// Construction / Destruction
// ============================================================================

GitResourceProvider::GitResourceProvider(const ResourceContext& ctx) {
    cache_dir_ = ctx.config.value("cacheDir", "cache/git");
    cache_ttl_seconds_ = ctx.config.value("cacheTtlSeconds", 604800);
    cleanup_interval_sec_ = ctx.config.value("cleanupIntervalSeconds", 3600);

    // Defensive bound: 0 / negative interval would spin the cleanup loop on the
    // CPU. ttl is allowed to be 0 (means "expire everything immediately").
    if(cleanup_interval_sec_ < 1) cleanup_interval_sec_ = 1;
    if(cache_ttl_seconds_ < 0) cache_ttl_seconds_ = 0;
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

bool GitResourceProvider::validate_config(const nlohmann::json& config, std::string& error) {
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
    // Idempotent guard. Without it a second start() would overwrite a still-
    // joinable cleanup_thread_, which calls std::terminate per the standard.
    if(started_) return;

    // Ensure cache directory exists. Fail loudly here - otherwise the cleanup
    // thread spins forever in a no-op `continue` and resolve() blows up on
    // its first call with an opaque git error. validate_config() only checks
    // the parent path, so creation can still legitimately fail at this point.
    std::error_code mk_ec;
    fs::create_directories(cache_dir_, mk_ec);
    if(mk_ec || !fs::is_directory(cache_dir_)) {
        throw std::runtime_error(
            "[GitProvider] Failed to create cacheDir '" + cache_dir_ + "': "
            + (mk_ec ? mk_ec.message() : std::string("path is not a directory"))
        );
    }

    // Make every git child non-interactive: prevents the credential helper
    // from blocking on stdin and surfaces auth failures as plain errors.
    // Set in the parent process so children inherit via _popen/popen.
    // The Unix-shell `VAR=value cmd` prefix does not work under cmd.exe.
    #ifdef _WIN32
    _putenv_s("GIT_TERMINAL_PROMPT", "0");
    #else
    setenv("GIT_TERMINAL_PROMPT", "0", 1);
    #endif

    started_ = true;
    stop_ = false;
    cleanup_thread_ = std::thread(&GitResourceProvider::cleanup_loop, this);
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

    // Validate inputs against shell injection.
    // `%` is critical on Windows: cmd.exe expands `%NAME%` even inside
    // double-quoted arguments, so a malicious URL like `https://%PATH%/`
    // would silently substitute env-var content into the git command line.
    // `^` is cmd.exe's escape character (only meaningful outside quotes,
    // but blocked defensively - it has no place in URLs or branch names).
    // `!` triggers cmd.exe delayed expansion (`!VAR!`) when the spawning
    // shell has `setlocal enabledelayedexpansion` active. `_popen` does
    // not enable it by default, but blocking is free defense in depth.
    // `=` would corrupt the `-c <name>=<value>` parse used by
    // `auth_override_arg`: git splits on the first `=`, so a `=` inside the
    // URL (or a base64-padded token) shifts the key/value boundary and
    // produces a malformed config entry.
    static const auto is_safe = [](const std::string& s) {
        for(char c : s) {
            if(c == '\0' || c == '"' || c == '\\' || c == '`' || c == '$'
                || c == '(' || c == ')' || c == '&' || c == '|' || c == ';'
                || c == '<' || c == '>' || c == '\'' || c == '\n' || c == '\r'
                || c == '%' || c == '^' || c == '!' || c == '=')
                return false;
        }
        return true;
    };
    if(!is_safe(url))
        throw std::runtime_error("[GitProvider] Invalid characters in URL");
    if(!branch.empty() && !is_safe(branch))
        throw std::runtime_error("[GitProvider] Invalid characters in branch name");
    if(!token.empty() && !is_safe(token))
        throw std::runtime_error("[GitProvider] Invalid characters in token");

    std::string cache_key = make_cache_key(url, branch);
    fs::path cache_path = fs::path(cache_dir_) / cache_key;

    // Get or create per-key mutex
    std::mutex* key_mutex;
    {
        std::lock_guard map_lock(key_mutexes_map_mutex_);
        key_mutex = &key_mutexes_[cache_key];
    }

    std::lock_guard key_lock(*key_mutex);

    // We deliberately never log the URL - it can carry user-info (oauth2:token@host
    // or user:password@host). `cache_key` (DJB2 hash + sanitized host prefix) is
    // safe to log and gives enough correlation across log lines for the same
    // (url, branch) pair without exposing credentials. The orchestrator (which
    // submitted the URL) already knows it and can correlate via job_id from the
    // surrounding Pipeline log line.
    if(!fs::is_directory(cache_path)) {
        std::cout << "[GitProvider] Cloning [" << cache_key << "] (branch=" << branch << ")...\n";
        if(!clone_repo(url, branch, token, cache_path)) {
            throw std::runtime_error("[GitProvider] Clone failed (cache_key=" + cache_key + ")");
        }
    } else {
        std::cout << "[GitProvider] Fetching [" << cache_key << "] (branch=" << branch << ")...\n";
        if(!fetch_repo(url, branch, token, cache_path)) {
            throw std::runtime_error("[GitProvider] Fetch failed (cache_key=" + cache_key + ")");
        }
    }

    write_cache_meta(cache_path, url, branch); // url without token
    return cache_path;
}

// ============================================================================
// Git operations
// ============================================================================

/// Inject oauth2 token into URL for git HTTP auth.
/// GitLab git-smart-http requires Basic Auth (oauth2:token@host), not Bearer headers.
/// Strips any pre-existing user-info from the input URL so that
/// `https://user@host/repo.git` does not produce a malformed `oauth2:tok@user@host/...`.
static std::string inject_token(const std::string& url, const std::string& token) {
    if(token.empty()) return url;

    auto strip_userinfo = [](const std::string& rest) -> std::string {
        // `rest` starts right after "scheme://". Drop any `userinfo@` prefix
        // that appears before the first '/' (i.e. inside the authority).
        auto at = rest.find('@');
        auto slash = rest.find('/');
        if(at != std::string::npos && (slash == std::string::npos || at < slash))
            return rest.substr(at + 1);
        return rest;
    };

    if(url.rfind("https://", 0) == 0)
        return "https://oauth2:" + token + "@" + strip_userinfo(url.substr(8));
    if(url.rfind("http://", 0) == 0)
        return "http://oauth2:" + token + "@" + strip_userinfo(url.substr(7));
    return url;
}

/// Strip oauth2 tokens from git output to prevent log leakage.
/// Handles both `oauth2:<tok>@host` (raw) and `oauth2%3A<tok>@host` (URL-encoded
/// - git sometimes prints credentials this way inside error messages).
static std::string strip_token(const std::string& text) {
    std::string r = text;
    for(const char* marker : {"oauth2:", "oauth2%3A", "oauth2%3a"}) {
        size_t marker_len = std::char_traits<char>::length(marker);
        size_t pos = 0;
        while((pos = r.find(marker, pos)) != std::string::npos) {
            size_t at = r.find('@', pos + marker_len);
            if(at != std::string::npos) {
                r.replace(pos + marker_len, at - pos - marker_len, "***");
                pos += marker_len + 3; // past the "***" replacement
            } else {
                pos += marker_len;
            }
        }
    }
    return r;
}


/// Build the `-c "url.<auth>.insteadOf=<plain>"` prefix that gives a single
/// git invocation network-only access to the auth URL, without ever writing
/// the token to `.git/config`. Returns an empty string when no token is set.
///
/// This collapses the old set-url/scrub dance - clone went 2->1 subprocess,
/// fetch went 4->2. The token is still briefly visible in /proc/<pid>/cmdline
/// or Windows Process Explorer for the duration of the git child, exactly as
/// before; the win is that nothing lands on disk.
static std::string auth_override_arg(const std::string& url, const std::string& token) {
    if(token.empty()) return {};
    std::string auth_url = inject_token(url, token);
    return " -c \"url." + auth_url + ".insteadOf=" + url + "\"";
}

// ============================================================================
// Retry policy for transient network failures
// ============================================================================

/// Total attempts (1 original + 4 retries) for a single git network operation.
static constexpr int kGitMaxAttempts = 5;

/// Classify a failed git command's output as permanent (don't retry) vs
/// transient (worth a retry). Match against substrings that are stable
/// across git versions and locales - we deliberately err on the side of
/// retrying anything unrecognised, since the cost of a needless retry is
/// small (15s worst case) compared to giving up on a flaky network.
static bool is_permanent_git_error(const std::string& output) {
    static constexpr const char* permanent_markers[] = {
        "Authentication failed",
        "could not read Username",         // GIT_TERMINAL_PROMPT=0 + missing creds
        "terminal prompts disabled",
        "Permission denied",
        "Repository not found",
        "repository '",                    // "repository '...' not found"
        "does not exist",
        "Remote branch",                   // "Remote branch X not found"
        "couldn't find remote ref",
        "HTTP 401",
        "HTTP 403",
        "HTTP 404",
        "fatal: bad",                      // "fatal: bad object", "fatal: bad config", etc.
        "Invalid characters in",           // our own pre-check (defensive - shouldn't reach here)
    };
    for(const char* marker : permanent_markers) {
        if(output.find(marker) != std::string::npos) return true;
    }
    return false;
}

/// Exponential backoff between attempts: 1s, 2s, 4s, 8s (total ~15s for 4 retries).
static std::chrono::milliseconds retry_backoff(int attempt /* 1-based of completed attempt */) {
    int seconds = 1;
    for(int i = 1; i < attempt; ++i) seconds *= 2;
    if(seconds > 8) seconds = 8;
    return std::chrono::seconds(seconds);
}

/// Run a single git network command up to `kGitMaxAttempts` times.
/// `tag` is a non-sensitive identifier (e.g. cache_key) used in log lines.
/// We deliberately never log the URL itself: it can carry user-info
/// (oauth2:token@host or user:password@host) and the orchestrator already
/// has it from the original task submission, so cache_key is enough to
/// correlate this log line with the surrounding pipeline job_id.
/// `pre_attempt` (optional) is invoked BEFORE each attempt - used by clone_repo
/// to wipe a partial destination directory before retrying.
/// Returns the final CommandResult (success of last attempt, or last failure).
template<typename PreAttempt>
static CommandResult run_with_retry(
    const std::string& op_name,
    const std::string& tag,
    const std::string& cmd,
    PreAttempt pre_attempt
) {
    CommandResult last;
    for(int attempt = 1; attempt <= kGitMaxAttempts; ++attempt) {
        pre_attempt(attempt);
        last = run_command(cmd);
        if(!last.failed()) {
            if(attempt > 1) {
                std::cout << "[GitProvider] " << op_name << " succeeded on attempt "
                    << attempt << "/" << kGitMaxAttempts << " [" << tag << "]\n";
            }
            return last;
        }
        if(is_permanent_git_error(last.output)) {
            std::cerr << "[GitProvider] " << op_name << " failed permanently (no retry) ["
                << tag << "]: " << strip_token(last.output) << "\n";
            return last;
        }
        if(attempt < kGitMaxAttempts) {
            auto wait = retry_backoff(attempt);
            std::cerr << "[GitProvider] " << op_name << " attempt " << attempt
                << "/" << kGitMaxAttempts << " failed [" << tag << "]"
                << " (retrying in " << wait.count() << "ms): "
                << strip_token(last.output) << "\n";
            std::this_thread::sleep_for(wait);
        }
    }
    std::cerr << "[GitProvider] " << op_name << " failed after "
        << kGitMaxAttempts << " attempts [" << tag << "]: "
        << strip_token(last.output) << "\n";
    return last;
}

bool GitResourceProvider::clone_repo(
    const std::string& url,
    const std::string& branch,
    const std::string& token,
    const fs::path& dest
) const {
    // Use forward-slash path form on every shell-bound command. cmd.exe is
    // happy with either separator inside double-quoted args, but a `\` right
    // before the closing `"` would be misread as an escaped quote by the
    // MSVCRT cmdline parser inside git.exe - `generic_string()` removes the
    // whole class of `\"` edge cases.
    const std::string dest_str = dest.generic_string();

    // Clone the *plain* URL (so it's what ends up in `.git/config`) and rely
    // on `insteadOf` to substitute the auth URL only for the network call.
    std::string cmd = "git" + auth_override_arg(url, token) + " clone --depth 1";
    if(!branch.empty()) {
        cmd += " --branch \"" + branch + "\"";
    }
    cmd += " \"" + url + "\" \"" + dest_str + "\" 2>&1";

    // Between attempts, wipe any partial clone - otherwise git refuses with
    // "destination path '...' already exists and is not an empty directory".
    auto wipe_dest = [&dest](int attempt) {
        if(attempt == 1) return;
        std::error_code ec;
        fs::remove_all(dest, ec);
    };

    // dest.filename() IS the cache_key (cache_path = cache_dir / cache_key),
    // and cache_key is a sanitized DJB2-derived string - safe for logs.
    auto result = run_with_retry("Clone", dest.filename().string(), cmd, wipe_dest);
    return !result.failed();
}

bool GitResourceProvider::fetch_repo(
    const std::string& url,
    const std::string& branch,
    const std::string& token,
    const fs::path& dest
) const {
    // See clone_repo for the rationale behind generic_string().
    const std::string dest_str = dest.generic_string();
    const std::string auth_arg = auth_override_arg(url, token);

    // Fetch - `insteadOf` rewrites the on-disk plain URL to the auth URL for
    // this single invocation; nothing touches `.git/config`. A failed fetch
    // leaves the working tree untouched, so no cleanup is needed between
    // retry attempts.
    std::string fetch_cmd = "git" + auth_arg
        + " -C \"" + dest_str + "\" fetch --depth 1 origin";
    if(!branch.empty()) {
        fetch_cmd += " \"" + branch + "\"";
    }
    fetch_cmd += " 2>&1";
    auto fetch_result = run_with_retry("Fetch", dest.filename().string(), fetch_cmd, [](int) {});
    if(fetch_result.failed()) {
        return false;
    }

    // Reset to FETCH_HEAD (purely local; no auth needed; no retry).
    std::string reset_cmd = "git -C \"" + dest_str + "\" reset --hard FETCH_HEAD 2>&1";
    auto reset_result = run_command(reset_cmd);
    if(reset_result.failed()) {
        std::cerr << "[GitProvider] Reset failed: " << strip_token(reset_result.output) << "\n";
        return false;
    }

    return true;
}

// ============================================================================
// Cache metadata
// ============================================================================

void GitResourceProvider::write_cache_meta(
    const fs::path& dir,
    const std::string& url,
    const std::string& branch
) {
    // Get current time as ISO8601 string. Use thread-safe gmtime variants -
    // std::gmtime returns a pointer to a shared static buffer and is racy
    // across concurrent resolve() calls.
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_utc{};
    #ifdef _WIN32
    gmtime_s(&tm_utc, &now_t);
    #else
    gmtime_r(&now_t, &tm_utc);
    #endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);

    nlohmann::json meta = {
        {"url", url},              // no token
        {"branch", branch},
        {"last_pull_at", buf}
    };

    fs::path meta_path = dir / ".cache_meta.json";
    std::ofstream f(meta_path);
    if(f.is_open()) {
        f << meta.dump(2) << "\n";
    } else {
        // Without metadata, cleanup_loop will treat this entry as ineligible
        // for TTL eviction (see the `fs::exists(meta_path)` check). That is
        // a silent leak of disk space - surface it in the log.
        std::cerr << "[GitProvider] Failed to write cache metadata: "
            << meta_path.string() << "\n";
    }
}

// ============================================================================
// Background cleanup
// ============================================================================

void GitResourceProvider::cleanup_loop() {
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
            // Reset `ec` per iteration. Otherwise a failed `fs::remove_all`
            // at the bottom of the loop body leaves `ec` set, and the next
            // iteration's `if(ec) break` aborts the entire cleanup scan
            // because of one stuck entry.
            ec.clear();
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
                tm.tm_isdst = 0;
                #ifdef _WIN32
                auto entry_time = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));
                #else
                auto entry_time = std::chrono::system_clock::from_time_t(timegm(&tm));
                #endif

                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry_time).count();
                if(age > cache_ttl_seconds_) {
                    std::string key = entry.path().filename().string();
                    // Hold the per-key mutex for the entire remove_all so a
                    // concurrent resolve() can't start cloning into the same
                    // directory while we are deleting it.
                    //
                    // Use `[]` (not `find`) so a fresh-from-disk entry - e.g.
                    // a stale cache dir from a previous server run, for which
                    // no resolve() has yet created a mutex - is still
                    // serialized: subsequent resolve() will use the same
                    // mutex address we lock here.
                    //
                    // We do NOT erase the entry afterwards: another resolve()
                    // may be waiting on the mutex (it captured the address
                    // before we acquired it), and destroying the mutex would
                    // be use-after-free. The map grows only by unique
                    // (url, branch) pairs, which is bounded in practice.
                    std::unique_lock<std::mutex> key_lock;
                    {
                        std::lock_guard map_lock(key_mutexes_map_mutex_);
                        key_lock = std::unique_lock<std::mutex>(
                            key_mutexes_[key],
                            std::try_to_lock
                        );
                        if(!key_lock.owns_lock()) {
                            continue;  // Active job - skip, retry next cycle
                        }
                    }
                    std::cout << "[GitProvider] Removing expired cache: " << key << "\n";
                    fs::remove_all(entry.path(), ec);
                }
            } catch(const std::exception& e) {
                std::cerr << "[GitProvider] Cleanup error for " << entry.path() << ": " << e.what() << "\n";
            }
        }
    }
}

REGISTER_RESOURCE_PROVIDER(GitResourceProvider, "git")
