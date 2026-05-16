#pragma once

/**
 * @file build_service.h
 * @brief Dynamically compiles runner executables and test plugins.
 *
 * Workflow: build_runner() compiles student solution + teacher's main.cpp into
 * a single "runner" executable. build_test_plugins() compiles test plugin DLLs
 * for parent-side setup/verify (no student code linked).
 *
 * Cross-platform: MinGW on Windows, GCC on Linux.
 */

#include <atomic>
#include <chrono>
#include <cmake_generator.h>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class BuildService {
    public:
        struct BuildConfig {
            std::string engine_lib_path;
            std::string engine_include_path;
            std::string parallel_lib_path;
            std::string parallel_include_path;
            std::string runner_lib_path;             ///< Path to librunner_lib.a (base, framework-agnostic)
            std::string
            runner_include_path;         ///< Path to runner_lib/include (the only public include for variants)
            std::string shadow_omp_dir;              ///< Path to parallel_lib/shadow/
            // Per-framework runner *sources* - compiled at student-build time with the
            // same compiler that compiles the student's solution (g++ for omp/parlay/seq,
            // OpenCilk clang for cilk). This lets each variant use its framework's
            // native keywords (e.g. cilk_for) for setup-time warmup.
            std::string runner_omp_source_path;      ///< Path to runner_omp.cpp source
            std::string runner_parlay_source_path;   ///< Path to runner_parlay.cpp source
            std::string runner_cilk_source_path;     ///< Path to runner_cilk.cpp source
            std::string runner_seq_source_path;      ///< Path to runner_seq.cpp source
            std::string parlay_headers_path;         ///< Path to ParlayLib headers (parent of parlay/)
            std::string template_dir;                ///< Dir containing runner_wrapper.cmake.in etc.
            std::string cmake_executable;
            std::string generator;
            std::string exe_dir;
            int correctness_workers = 4;
            long long default_memory_limit_mb = 1024;
            int default_threads = 4;                  ///< Per-job fallback for `threads`.
            int default_wall_time_sec = 60;           ///< Per-job fallback for `wallTimeSec`.
            int default_cpu_time_sec = 30;            ///< Per-job fallback for `cpuTimeSec`.
            int sandbox_process_multiplier = 2;       ///< max_processes = threads * multiplier.
        };

        /** @brief Result of building the runner executable (student + teacher main). */
        struct RunnerBuildResult {
            bool success = false;
            std::string error_message;
            std::string build_output;
            std::string runner_exe_path;          ///< Path to built runner executable.
            std::string build_dir;                ///< Temp dir (for cleanup).
        };

        /** @brief Result of building test plugin DLLs (parent-side setup/verify). */
        struct TestPluginBuildResult {
            bool success = false;
            std::string error_message;
            std::string build_output;
            std::vector<std::string> plugin_paths;   ///< Built plugin DLLs.
            std::string build_dir;                    ///< Temp dir (for cleanup).
        };

        explicit BuildService(BuildConfig config);

        ~BuildService();

        BuildService(const BuildService&) = delete;
        BuildService& operator=(const BuildService&) = delete;

        /** @brief Build a runner executable from student solution + teacher's main.cpp.
         *  Network is BLOCKED via network_block_script(). The student's solution/CMakeLists.txt
         *  is used as-is - engine never replaces or substitutes it. */
        RunnerBuildResult build_runner(
            const std::string& framework,         ///< "openmp", "parlay", or "cilk"
            const std::string& solution_dir,
            const std::string& test_dir,          ///< For runner/main.cpp and include/
            const std::string& job_id,
            bool shadow_omp = true                ///< include shadow_omp/ (forbid raw <omp.h>)
        );

        /// Returns extra library directories the runner process needs visible at
        /// runtime (bind-mounted into the sandbox + LD_LIBRARY_PATH). Currently
        /// only populated for cilk (OpenCilk runtime libs).
        std::vector<std::string> get_extra_lib_dirs(const std::string& framework) const;

        /** @brief Build test plugin DLLs for parent-side setup/verify.
         *  Plugins link test_engine + runner_lib. NO student code. */
        TestPluginBuildResult build_test_plugins(
            const std::string& test_dir
        );

        void cleanup(const std::string& build_dir);

        /** @brief Check that all required paths/tools exist for the given framework.
         *  Returns {true, ""} on success or {false, error_message} on failure. */
        std::pair<bool, std::string> validate_framework(const std::string& framework) const;

        /// Static/initial config (paths, generator, etc). Treat the scalar
        /// "default_*" fields here as initial values only - read the LIVE
        /// values through the atomic accessors below.
        const BuildConfig& config() const { return config_; }

        // ---- Live (thread-safe) per-job defaults ----------------------------
        // Written by adapters via TestRunnerService setters (e.g. PUT /api/config),
        // read by worker threads through Pipeline::init_job_context. All four
        // are protected by std::atomic to avoid data races.

        long long default_memory_limit_mb() const { return default_memory_limit_mb_.load(std::memory_order_relaxed); }
        int default_threads() const { return default_threads_.load(std::memory_order_relaxed); }
        int default_wall_time_sec() const { return default_wall_time_sec_.load(std::memory_order_relaxed); }
        int default_cpu_time_sec() const { return default_cpu_time_sec_.load(std::memory_order_relaxed); }
        int sandbox_process_multiplier() const { return sandbox_process_multiplier_.load(std::memory_order_relaxed); }

        void set_default_memory_limit_mb(long long v) { default_memory_limit_mb_.store(v, std::memory_order_relaxed); }
        void set_default_threads(int v) { default_threads_.store(v, std::memory_order_relaxed); }
        void set_default_wall_time_sec(int v) { default_wall_time_sec_.store(v, std::memory_order_relaxed); }
        void set_default_cpu_time_sec(int v) { default_cpu_time_sec_.store(v, std::memory_order_relaxed); }
        void set_sandbox_process_multiplier(int v) { sandbox_process_multiplier_.store(v, std::memory_order_relaxed); }

    private:
        /// Cached test-plugin build keyed on the test directory.
        /// Plugins depend only on test_dir contents, so a same-test_dir submission
        /// can reuse a previously built set of plugin DLLs as long as no source
        /// file has been touched (mtime unchanged). LRU-evicted at kPluginCacheMax
        /// to keep the disk footprint bounded for long-running servers.
        struct PluginCacheEntry {
            std::vector<std::string> plugin_paths;  ///< Paths to plugin_*.dll/.so
            std::string build_dir;                  ///< Temp dir owned by the cache
            long long mtime_ns = 0;                 ///< Max mtime of test_dir files at build time
            std::chrono::steady_clock::time_point last_access;
        };

        static constexpr size_t kPluginCacheMax = 32;

        BuildConfig config_;
        CMakeGenerator cmake_gen_;
        std::string cilk_compiler_path_;             ///< Cached OpenCilk clang++ path; empty if not found.
        std::string
        cilk_runtime_dir_;               ///< Cached `<cilk_compiler> -print-runtime-dir`; empty if not available.

        // Live per-job defaults, atomic to avoid data race with concurrent
        // worker-thread reads in Pipeline::init_job_context.
        std::atomic<long long> default_memory_limit_mb_{1024};
        std::atomic<int> default_threads_{4};
        std::atomic<int> default_wall_time_sec_{60};
        std::atomic<int> default_cpu_time_sec_{30};
        std::atomic<int> sandbox_process_multiplier_{2};

        std::mutex plugin_cache_mutex_;
        std::map<std::string, PluginCacheEntry> plugin_cache_;
};
