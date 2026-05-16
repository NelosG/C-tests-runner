#include <algorithm>
#include <atomic>
#include <build_service.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <log_utils.h>
#include <process_utils.h>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;


namespace {

    #ifdef _WIN32
    constexpr const char* SHARED_LIB_EXT = ".dll";
    constexpr const char* EXE_EXT = ".exe";
    #else
    constexpr const char* SHARED_LIB_EXT = ".so";
    constexpr const char* EXE_EXT = "";
    #endif

    std::atomic<int> temp_counter{0};

    // ------------------------------------------------------------------
    // Filesystem helpers
    // ------------------------------------------------------------------

    /// Reliable recursive directory copy (fs::copy with recursive is unreliable on MinGW).
    /// Skips symlinks to defend against symlink attacks from cloned repos.
    void copy_directory_recursive(const fs::path& src, const fs::path& dst, std::error_code& ec) {
        fs::create_directories(dst, ec);
        if(ec) return;

        for(const auto& entry : fs::directory_iterator(src, ec)) {
            if(ec) return;
            if(entry.is_symlink()) continue;
            const auto& target = dst / entry.path().filename();
            if(entry.is_directory()) {
                copy_directory_recursive(entry.path(), target, ec);
                if(ec) return;
            } else {
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
                if(ec) return;
            }
        }
    }

    /// One-shot scan at startup: remove orphaned engine temp dirs left over
    /// from crashed / kill -9'd previous runs. Only directories older than the
    /// threshold are removed, so a concurrent server on the same box (rare)
    /// doesn't get its in-flight job dirs blown away.
    void sweep_stale_temp_dirs() {
        const fs::path temp = fs::temp_directory_path();
        const auto threshold = std::chrono::hours(1);
        const auto now = fs::file_time_type::clock::now();

        std::error_code iter_ec;
        int removed = 0;
        for(auto it = fs::directory_iterator(temp, iter_ec);
            !iter_ec && it != fs::directory_iterator();
            it.increment(iter_ec)) {
            std::error_code op_ec;
            // Skip symlinks BEFORE following them: an attacker who creates
            // /tmp/build-runner-evil -> /etc would otherwise have us nuke /etc.
            if(it->is_symlink(op_ec) || op_ec) continue;
            if(!it->is_directory(op_ec) || op_ec) continue;

            const std::string name = it->path().filename().string();
            const bool engine_owned =
                name.rfind("build-runner-", 0) == 0
                || name.rfind("tests-", 0) == 0
                || name.rfind("ctr-sandbox-", 0) == 0;
            if(!engine_owned) continue;

            auto mtime = fs::last_write_time(it->path(), op_ec);
            if(op_ec) continue;
            if(now - mtime < threshold) continue;

            std::error_code rm_ec;
            fs::remove_all(it->path(), rm_ec);
            if(!rm_ec) ++removed;
        }
        if(removed > 0) {
            LOG("Build") << "Swept " << removed << " stale temp dir(s) from "
                << temp.string() << "\n";
        }
    }

    fs::path make_fresh_temp_dir(const std::string& prefix, const std::string& job_id) {
        fs::path tmp = fs::temp_directory_path() / (prefix + job_id);
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp);
        return tmp;
    }

    fs::path make_fresh_plugins_dir() {
        auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        fs::path tmp = fs::temp_directory_path()
            / ("tests-" + std::to_string(stamp) + "-" + std::to_string(temp_counter.fetch_add(1)));
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp);
        return tmp;
    }

    /// Returns the max last_write_time across regular files under `dir`,
    /// recursively, in nanoseconds. Used as a cheap "has anything changed?"
    /// fingerprint for the test-plugin cache.
    /// Per-entry filesystem errors are silently skipped - the iterator's own
    /// error_code (`iter_ec`) controls loop termination so a transient read
    /// failure on one file does not abort the whole scan.
    long long compute_max_mtime(const fs::path& dir) {
        long long max_ns = 0;
        std::error_code iter_ec;
        for(auto it = fs::recursive_directory_iterator(dir, iter_ec);
            !iter_ec && it != fs::recursive_directory_iterator();
            it.increment(iter_ec)) {
            std::error_code op_ec;
            if(!it->is_regular_file(op_ec) || op_ec) continue;
            auto t = fs::last_write_time(it->path(), op_ec);
            if(op_ec) continue;
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                t.time_since_epoch()
            ).count();
            if(ns > max_ns) max_ns = ns;
        }
        return max_ns;
    }

    // ------------------------------------------------------------------
    // CMake invocation
    // ------------------------------------------------------------------

    /// Returns the shell prefix that runs the following command inside a fresh
    /// network namespace ("unshare --net -r -- ") or "" when unavailable.
    ///
    /// The kernel-level netns drops outbound network for the cmake configure /
    /// build subprocess: any FetchContent / git clone / curl / find_package
    /// that tries to reach a host fails with ENETUNREACH. This is the primary
    /// defence on Linux against student CMake files that bypass the regex
    /// CMakeValidator (e.g. via include() of an absolute path). The in-CMake
    /// block_network.cmake remains as a second layer + Windows-only defence.
    ///
    /// Probe runs once at first invocation; result cached for the engine's
    /// lifetime. -r maps current uid to 0 inside a user-namespace so the call
    /// works both as root (privileged docker) and unprivileged (WSL/dev box).
    const std::string& network_isolated_prefix() {
        #ifndef __linux__
        static const std::string none;
        return none;
        #else
        static const std::string prefix = [] {
            auto r = run_command("unshare --net -r /bin/true 2>&1");
            if(!r.failed()) {
                LOG("Build") << "Build-time network isolation active "
                    "(unshare --net -r)\n";
                return std::string("unshare --net -r -- ");
            }
            LOG("Build") << "unshare --net probe failed - falling back to "
                "block_network.cmake only. Output: " << r.output;
            return std::string();
        }();
        return prefix;
        #endif
    }

    std::string cmake_configure_and_build(
        const std::string& cmake_executable,
        const std::string& generator,
        const fs::path& source_dir,
        const fs::path& build_dir,
        std::string& error_message,
        const std::string& extra_defines = ""
    ) {
        std::string output;
        const std::string& netns = network_isolated_prefix();

        std::string configure_cmd = netns
            + shell_quote(cmake_executable) + " "
            + "-G " + shell_quote(generator) + " "
            + "-DCMAKE_BUILD_TYPE=" CTR_BUILD_TYPE " "
            + extra_defines
            + shell_quote(source_dir.string()) + " "
            + "-B " + shell_quote(build_dir.string())
            + " 2>&1";
        auto cfg = run_command(configure_cmd);
        output += "=== Configure ===\n" + cfg.output + "\n";
        if(cfg.failed()) {
            error_message = "CMake configure failed:\n" + cfg.output;
            return output;
        }

        std::string build_cmd = netns
            + shell_quote(cmake_executable) + " "
            + "--build " + shell_quote(build_dir.string()) + " "
            + "--config " CTR_BUILD_TYPE
            + " 2>&1";
        auto bld = run_command(build_cmd);
        output += "=== Build ===\n" + bld.output + "\n";
        if(bld.failed()) {
            error_message = "Build failed:\n" + bld.output;
        }
        return output;
    }

    // ------------------------------------------------------------------
    // OpenCilk compiler discovery
    // ------------------------------------------------------------------

    /// Probe whether the given clang++ supports -fopencilk. Guards against the
    /// "OPENCILK_PATH points to a vanilla clang" trap.
    bool supports_fopencilk(const std::string& compiler_path) {
        auto probe = run_command(
            shell_quote(compiler_path)
            + " -fopencilk -E -x c++ "
            #ifdef _WIN32
            "NUL"
            #else
            "/dev/null"
            #endif
            " 2>&1"
        );
        // Compiler that doesn't know -fopencilk prints "unrecognized option" / "unknown argument"
        // and exits non-zero with the flag name in the message.
        return !probe.failed() || probe.output.find("fopencilk") == std::string::npos;
    }

    /// Ask the OpenCilk clang where its runtime lib dir is - the directory that
    /// holds libopencilk.so.1, libopencilk-personality-cpp.so.1 etc. This dir is
    /// outside the sandbox by default; the result is bind-mounted via JobConfig.
    /// Returns empty string if the probe fails or the directory does not exist.
    std::string find_cilk_runtime_dir(const std::string& compiler_path) {
        if(compiler_path.empty()) return {};
        auto probe = run_command(shell_quote(compiler_path) + " -print-runtime-dir 2>&1");
        if(probe.failed()) return {};
        std::string dir = probe.output;
        while(!dir.empty()
            && (dir.back() == '\n' || dir.back() == '\r'
                || dir.back() == ' ' || dir.back() == '\t')) {
            dir.pop_back();
        }
        if(dir.empty() || !fs::is_directory(dir)) return {};
        return dir;
    }

    /// Locate an OpenCilk-aware clang++. Checks (in order):
    ///   1. $OPENCILK_PATH/bin/clang++
    ///   2. /opt/opencilk/bin/clang++
    /// Each candidate is probed for -fopencilk support; non-OpenCilk clangs at
    /// these paths are skipped. "which clang++" is intentionally NOT used -
    /// system clang typically does not support -fopencilk.
    /// Returns empty string if no working candidate exists.
    std::string find_opencilk_compiler() {
        std::vector<std::string> candidates;
        if(const char* env = std::getenv("OPENCILK_PATH")) {
            candidates.push_back(std::string(env) + "/bin/clang++");
        }
        candidates.push_back("/opt/opencilk/bin/clang++");

        for(const auto& path : candidates) {
            if(!fs::exists(path)) continue;
            if(!supports_fopencilk(path)) {
                LOG_ERR("Build") << "Compiler " << path
                    << " does not support -fopencilk (not OpenCilk); skipping\n";
                continue;
            }
            return path;
        }
        return {};
    }

    // ------------------------------------------------------------------
    // Runner-build pipeline steps (each operates on the temp workspace)
    // ------------------------------------------------------------------

    struct RunnerWorkspace {
        fs::path root;            ///< temp dir holding wrapper CMakeLists + solution/
        fs::path solution;        ///< root/solution - copied from solution_dir
        fs::path build;           ///< root/build  - CMake build tree
    };

    bool stage_student_solution(
        const fs::path& solution_dir,
        RunnerWorkspace& ws,
        std::string& error_out
    ) {
        std::error_code ec;
        copy_directory_recursive(solution_dir, ws.solution, ec);
        if(ec) {
            error_out = "Failed to copy solution to workspace: " + ec.message();
            return false;
        }
        if(!fs::exists(ws.solution / "CMakeLists.txt")) {
            error_out = "Student solution must contain CMakeLists.txt at solution root";
            return false;
        }
        return true;
    }

    /// Copy the static `block_network.cmake` from the engine's template dir
    /// into the workspace, so the wrapper CMakeLists can `include(...)` it.
    void write_network_blocker(
        const RunnerWorkspace& ws,
        const std::string& template_dir
    ) {
        fs::path src = fs::path(template_dir) / "block_network.cmake";
        std::error_code ec;
        if(fs::exists(src, ec) && !ec) {
            fs::copy_file(
                src,
                ws.root / "block_network.cmake",
                fs::copy_options::overwrite_existing,
                ec
            );
            if(!ec) return;
        }
        // Fallback: write a minimal blocker if the file isn't shipped (dev
        // builds where cmake/ wasn't deployed yet).
        std::ofstream f(ws.root / "block_network.cmake");
        f << "set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL \"\" FORCE)\n";
    }

    struct TeacherTestArtifacts {
        fs::path runner_main;
        std::string include_dir;
        std::vector<std::string> extra_lib_dirs;
    };

    bool collect_teacher_test_artifacts(
        const std::string& test_dir,
        TeacherTestArtifacts& out,
        std::string& error_out
    ) {
        out.runner_main = fs::absolute(fs::path(test_dir) / "runner" / "main.cpp");
        if(!fs::exists(out.runner_main)) {
            error_out = "runner/main.cpp not found in test directory: " + test_dir;
            return false;
        }
        out.include_dir = fs::absolute(fs::path(test_dir) / "include").string();

        fs::path libs_dir = fs::absolute(fs::path(test_dir) / "libs");
        std::error_code dir_ec;
        if(fs::is_directory(libs_dir, dir_ec) && !dir_ec) {
            std::error_code iter_ec;
            for(auto it = fs::directory_iterator(libs_dir, iter_ec);
                !iter_ec && it != fs::directory_iterator();
                it.increment(iter_ec)) {
                std::error_code op_ec;
                if(it->is_directory(op_ec) && !op_ec) {
                    out.extra_lib_dirs.push_back(it->path().string());
                }
            }
            out.extra_lib_dirs.push_back(libs_dir.string());
        }
        return true;
    }

    /// Compose -D definitions for the cmake configure step.
    /// For cilk, sets CMAKE_CXX_COMPILER / CMAKE_C_COMPILER to the cached OpenCilk clang.
    std::string build_cmake_defines(
        const std::string& framework,
        const std::string& cilk_compiler_path
    ) {
        std::string defines =
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON "
            "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON ";
        if(framework == "cilk" && !cilk_compiler_path.empty()) {
            std::string cilk_c = cilk_compiler_path;
            auto pos = cilk_c.rfind("clang++");
            if(pos != std::string::npos) cilk_c.replace(pos, 7, "clang");
            // shell_quote the path portion so a compiler installed under a
            // path with spaces (e.g. "/opt/open cilk/bin/clang++") survives
            // the run_command() shell pass without being split into two args.
            defines += "-DCMAKE_CXX_COMPILER=" + shell_quote(cilk_compiler_path) + " "
                + "-DCMAKE_C_COMPILER=" + shell_quote(cilk_c) + " ";
        }
        return defines;
    }

    /// Locate the freshly built runner exe inside the build tree.
    std::string find_runner_exe(const fs::path& build_dir) {
        std::string runner_name = std::string("runner") + EXE_EXT;
        std::error_code iter_ec;
        for(auto it = fs::recursive_directory_iterator(build_dir, iter_ec);
            !iter_ec && it != fs::recursive_directory_iterator();
            it.increment(iter_ec)) {
            std::error_code op_ec;
            if(it->is_regular_file(op_ec) && !op_ec
                && it->path().filename() == runner_name) {
                return it->path().string();
            }
        }
        return {};
    }

    /// Copy framework-specific runtime libs (currently only parallel_lib for openmp)
    /// next to the runner exe so the sandbox can load them. test_engine is intentionally
    /// NOT copied - it is loaded only by test plugins, never by the runner exe.
    void copy_runner_runtime_deps(
        const std::string& runner_exe,
        const std::string& framework,
        const std::string& parallel_lib_path
    ) {
        if(framework != "openmp" || parallel_lib_path.empty()) return;
        fs::path src(parallel_lib_path);
        if(!fs::exists(src)) return;
        fs::path dst = fs::path(runner_exe).parent_path() / src.filename();
        if(fs::exists(dst)) return;
        std::error_code ec;
        fs::copy_file(src, dst, fs::copy_options::skip_existing, ec);
    }

} // anonymous namespace

// ============================================================================
// BuildService construction & framework validation
// ============================================================================

BuildService::BuildService(BuildConfig config)
    : config_(config),
      cmake_gen_(
          CMakeGenerator::Config{
              config.engine_lib_path,
              config.engine_include_path,
              config.parallel_lib_path,
              config.parallel_include_path,
              config.runner_lib_path,
              config.runner_include_path,
              config.shadow_omp_dir,
              config.runner_omp_source_path,
              config.runner_parlay_source_path,
              config.runner_cilk_source_path,
              config.runner_seq_source_path,
              config.parlay_headers_path,
              config.template_dir
          }
      ),
      cilk_compiler_path_(find_opencilk_compiler()),
      cilk_runtime_dir_(find_cilk_runtime_dir(cilk_compiler_path_)) {
    // Seed live atomic defaults from the initial config snapshot.
    default_memory_limit_mb_.store(config_.default_memory_limit_mb, std::memory_order_relaxed);
    default_threads_.store(config_.default_threads, std::memory_order_relaxed);
    default_wall_time_sec_.store(config_.default_wall_time_sec, std::memory_order_relaxed);
    default_cpu_time_sec_.store(config_.default_cpu_time_sec, std::memory_order_relaxed);
    sandbox_process_multiplier_.store(config_.sandbox_process_multiplier, std::memory_order_relaxed);

    if(cilk_compiler_path_.empty()) {
        LOG("Build") << "OpenCilk compiler not found; cilk framework will be unavailable. "
            "Set OPENCILK_PATH env or install to /opt/opencilk to enable.\n";
    } else {
        LOG("Build") << "OpenCilk compiler: " << cilk_compiler_path_ << "\n";
        if(!cilk_runtime_dir_.empty())
            LOG("Build") << "OpenCilk runtime dir: " << cilk_runtime_dir_ << "\n";
        else
            LOG("Build") << "OpenCilk runtime dir not detected (-print-runtime-dir failed); "
                "runner may fail to start in sandbox.\n";
    }
    // Reclaim disk from previous runs that didn't get a chance to run their
    // destructors (kill -9, OS crash, container restart, ...).
    sweep_stale_temp_dirs();
}

BuildService::~BuildService() {
    // Clear the test-plugin cache: each entry owns a temp build dir.
    std::lock_guard lock(plugin_cache_mutex_);
    for(auto& [_, entry] : plugin_cache_) {
        if(!entry.build_dir.empty()) cleanup(entry.build_dir);
    }
    plugin_cache_.clear();
}

std::pair<bool, std::string> BuildService::validate_framework(const std::string& framework) const {
    if(!fs::exists(config_.engine_lib_path))
        return {false, "test_engine library not found: " + config_.engine_lib_path};
    if(!fs::exists(config_.runner_lib_path))
        return {false, "runner_lib not found: " + config_.runner_lib_path};

    if(framework == "openmp") {
        if(!fs::exists(config_.parallel_lib_path))
            return {false, "parallel_lib not found: " + config_.parallel_lib_path};
        if(!fs::exists(config_.runner_omp_source_path))
            return {false, "runner_omp source not found: " + config_.runner_omp_source_path};
        if(!fs::is_directory(config_.shadow_omp_dir))
            return {false, "shadow_omp dir not found: " + config_.shadow_omp_dir};
    } else if(framework == "parlay") {
        if(!fs::exists(config_.runner_parlay_source_path))
            return {
                false,
                "runner_parlay source not found (build with -DENABLE_PARLAY=ON): "
                + config_.runner_parlay_source_path
            };
        if(config_.parlay_headers_path.empty() || !fs::is_directory(config_.parlay_headers_path))
            return {false, "ParlayLib headers not found"};
    } else if(framework == "cilk") {
        if(!fs::exists(config_.runner_cilk_source_path))
            return {false, "runner_cilk source not found: " + config_.runner_cilk_source_path};
        if(cilk_compiler_path_.empty())
            return {false, "OpenCilk compiler not found. Set OPENCILK_PATH env or install to /opt/opencilk"};
    } else if(framework == "none") {
        if(!fs::exists(config_.runner_seq_source_path))
            return {false, "runner_seq source not found: " + config_.runner_seq_source_path};
    } else {
        return {false, "Unknown framework: " + framework};
    }
    return {true, ""};
}

std::vector<std::string> BuildService::get_extra_lib_dirs(const std::string& framework) const {
    std::vector<std::string> dirs;
    if(framework == "cilk" && !cilk_runtime_dir_.empty()) {
        dirs.push_back(cilk_runtime_dir_);
    }
    return dirs;
}

void BuildService::cleanup(const std::string& build_dir) {
    if(build_dir.empty()) return;
    // Retry: on Windows, files may still be locked briefly after process exit.
    for(int attempt = 0; attempt < 3; ++attempt) {
        std::error_code ec;
        fs::remove_all(build_dir, ec);
        if(!ec) return;
        if(attempt < 2) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// build_runner -- compiles student solution + teacher's main.cpp into runner exe
// ============================================================================

BuildService::RunnerBuildResult BuildService::build_runner(
    const std::string& framework,
    const std::string& solution_dir,
    const std::string& test_dir,
    const std::string& job_id,
    bool shadow_omp
) {
    RunnerBuildResult result;

    LOG("Build") << "Building runner for job " << job_id
        << " (framework=" << framework << ")\n";

    RunnerWorkspace ws;
    ws.root = make_fresh_temp_dir("build-runner-", job_id);
    ws.solution = ws.root / "solution";
    ws.build = ws.root / "build";
    result.build_dir = ws.root.string();

    if(!stage_student_solution(solution_dir, ws, result.error_message))
        return result;

    write_network_blocker(ws, config_.template_dir);

    TeacherTestArtifacts test_artifacts;
    if(!collect_teacher_test_artifacts(test_dir, test_artifacts, result.error_message))
        return result;

    {
        std::string cmake_text = cmake_gen_.runner_cmake_lists(
            framework,
            test_artifacts.runner_main.string(),
            test_artifacts.include_dir,
            test_artifacts.extra_lib_dirs,
            shadow_omp
        );
        std::ofstream f(ws.root / "CMakeLists.txt");
        f << cmake_text;
    }

    result.build_output = cmake_configure_and_build(
        config_.cmake_executable,
        config_.generator,
        ws.root,
        ws.build,
        result.error_message,
        build_cmake_defines(framework, cilk_compiler_path_)
    );
    if(!result.error_message.empty()) return result;

    result.runner_exe_path = find_runner_exe(ws.build);
    if(result.runner_exe_path.empty()) {
        result.error_message = "No runner executable found after build";
        return result;
    }

    copy_runner_runtime_deps(result.runner_exe_path, framework, config_.parallel_lib_path);

    result.success = true;
    LOG("Build") << "Runner build successful for job " << job_id << "\n";
    return result;
}

// ============================================================================
// build_test_plugins -- compiles teacher's test sources into setup/verify plugin DLLs
// ============================================================================

BuildService::TestPluginBuildResult BuildService::build_test_plugins(
    const std::string& test_dir
) {
    TestPluginBuildResult result;

    if(!fs::exists(test_dir)) {
        result.error_message = "Test directory not found: " + test_dir;
        return result;
    }

    std::string abs_test_dir = fs::absolute(fs::path(test_dir)).string();
    long long current_mtime = compute_max_mtime(test_dir);

    // ---- Cache lookup: test plugins depend only on test_dir contents.
    // Take a snapshot under the mutex, do the slow filesystem checks outside.
    std::vector<std::string> hit_paths;
    std::string stale_dir;
    {
        std::lock_guard lock(plugin_cache_mutex_);
        auto it = plugin_cache_.find(abs_test_dir);
        if(it != plugin_cache_.end()) {
            if(it->second.mtime_ns == current_mtime) {
                it->second.last_access = std::chrono::steady_clock::now();   // LRU bump
                hit_paths = it->second.plugin_paths;       // copy out for verification
            } else {
                stale_dir = it->second.build_dir;          // mtime changed -> drop
                plugin_cache_.erase(it);
            }
        }
    }
    if(!stale_dir.empty()) cleanup(stale_dir);

    if(!hit_paths.empty()) {
        bool all_present = true;
        for(const auto& p : hit_paths) {
            if(!fs::exists(p)) {
                all_present = false;
                break;
            }
        }
        if(all_present) {
            LOG("Build") << "Test plugins cache hit for " << abs_test_dir << "\n";
            result.success = true;
            result.plugin_paths = std::move(hit_paths);
            return result;
        }
        // DLL files vanished underneath us - drop the stale entry, fall through to rebuild.
        std::string vanished_dir;
        {
            std::lock_guard lock(plugin_cache_mutex_);
            auto it = plugin_cache_.find(abs_test_dir);
            if(it != plugin_cache_.end() && it->second.mtime_ns == current_mtime) {
                vanished_dir = it->second.build_dir;
                plugin_cache_.erase(it);
            }
        }
        if(!vanished_dir.empty()) cleanup(vanished_dir);
    }

    fs::path tmp = make_fresh_plugins_dir();
    result.build_dir = tmp.string();
    LOG("Build") << "Building test plugins...\n";

    std::string test_include_dir = (fs::path(abs_test_dir) / "include").string();

    {
        std::ofstream f(tmp / "CMakeLists.txt");
        f << cmake_gen_.test_plugin_cmake_lists(abs_test_dir, test_include_dir);
    }

    fs::path build_dir = tmp / "build";
    result.build_output = cmake_configure_and_build(
        config_.cmake_executable,
        config_.generator,
        tmp,
        build_dir,
        result.error_message
    );
    if(!result.error_message.empty()) {
        result.build_output += "[BuildService] Test plugins build failed\n";
        return result;
    }

    {
        std::error_code iter_ec;
        for(auto it = fs::recursive_directory_iterator(build_dir, iter_ec);
            !iter_ec && it != fs::recursive_directory_iterator();
            it.increment(iter_ec)) {
            std::error_code op_ec;
            if(!it->is_regular_file(op_ec) || op_ec) continue;
            if(it->path().extension().string() != SHARED_LIB_EXT) continue;
            std::string filename = it->path().filename().string();
            // Match both "plugin_*.dll/.so" and "libplugin_*.so" - teacher
            // may or may not set PREFIX "" on the target.
            const bool plugin_prefix = filename.rfind("plugin_", 0) == 0;
            const bool libplugin_prefix = filename.rfind("libplugin_", 0) == 0;
            if(plugin_prefix || libplugin_prefix) {
                result.plugin_paths.push_back(it->path().string());
            }
        }
    }

    // Empty result is fine - the assignment may use only JSON scenarios in
    // tests/cases/*.json without any C++ plugin DLLs.
    result.success = true;
    LOG("Build") << "Test plugins built: " << result.plugin_paths.size() << "\n";

    // ---- Hand the build dir over to the cache; the caller must NOT cleanup.
    // Race-safe overwrite + LRU eviction. All evicted dirs cleaned up outside the lock.
    std::vector<std::string> dirs_to_cleanup;
    {
        std::lock_guard lock(plugin_cache_mutex_);

        // Race-safe overwrite: if another thread already inserted for this key,
        // collect its build_dir for cleanup.
        auto it = plugin_cache_.find(abs_test_dir);
        if(it != plugin_cache_.end() && !it->second.build_dir.empty()
            && it->second.build_dir != result.build_dir) {
            dirs_to_cleanup.push_back(std::move(it->second.build_dir));
        }
        plugin_cache_[abs_test_dir] = PluginCacheEntry{
            result.plugin_paths,
            result.build_dir,
            current_mtime,
            std::chrono::steady_clock::now()
        };

        // LRU eviction: trim down to kPluginCacheMax. Never evict the entry we
        // just inserted (special-case in the comparator).
        while(plugin_cache_.size() > kPluginCacheMax) {
            auto oldest = std::min_element(
                plugin_cache_.begin(),
                plugin_cache_.end(),
                [&](const auto& a, const auto& b) {
                    if(a.first == abs_test_dir) return false;
                    if(b.first == abs_test_dir) return true;
                    return a.second.last_access < b.second.last_access;
                }
            );
            if(oldest == plugin_cache_.end() || oldest->first == abs_test_dir) break;
            if(!oldest->second.build_dir.empty()) {
                dirs_to_cleanup.push_back(std::move(oldest->second.build_dir));
            }
            plugin_cache_.erase(oldest);
        }
    }
    for(const auto& dir : dirs_to_cleanup) cleanup(dir);

    result.build_dir.clear();   // signal: don't auto-cleanup
    return result;
}
