#pragma once

/**
 * @file sandbox_launcher.h
 * @brief Sandboxed process execution for test runners.
 *
 * Launches runner executables in isolated environments:
 * - Linux: isolate (IOI sandbox) with cgroup limits
 * - Windows: CreateProcess + Job Object with memory/CPU/affinity limits
 */

#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class SandboxLauncher {
    public:
        struct Config {
            std::string isolate_path = "/usr/bin/isolate";  // Linux only
            int max_box_id = 99;
        };

        struct JobConfig {
            int wall_time_sec = 60;
            int cpu_time_sec = 30;
            int memory_limit_kb = 1048576;    // 1GB default
            int max_processes = 64;           // OMP threads
            std::string cpus;                 // "4-7" for cpuset (empty = no pinning)

            /// Extra host directories that must be visible to the runner process.
            /// Each is bind-mounted as /extra-libs-N inside the sandbox and prepended
            /// to LD_LIBRARY_PATH. Used for framework-specific runtime libs that live
            /// outside /runner (e.g. /opt/opencilk/lib/.../libopencilk*.so*).
            std::vector<std::string> extra_lib_dirs;

            /// Untimed warmup iterations the runner runs before the timed
            /// RUNNER_EXECUTE pass. Passed via --warmup; runner_lib reads
            /// runner::config().warmup_iterations.
            int warmup_iterations = 0;
        };

        struct RunResult {
            int exit_code = -1;
            bool timed_out = false;
            bool oom_killed = false;
            double wall_time_sec = 0.0;
            double cpu_time_sec = 0.0;
            long long cg_mem_peak_kb = 0;
            long long max_rss_kb = 0;
            std::string stderr_output;
        };

        explicit SandboxLauncher(Config config);

        // Execute runner in sandbox. Returns RunResult + parsed output.json (if available).
        std::pair<RunResult, std::optional<nlohmann::json>> execute(
            const std::string& runner_exe_path,
            const std::filesystem::path& input_dir,
            const std::filesystem::path& output_dir,
            int thread_count,
            const std::string& monitor_mode,   // "stress"|"monitor"|"normal"
            const JobConfig& job_config
        );

    private:
        Config config_;
        std::mutex box_mutex_;
        std::set<int> available_boxes_;

        int acquire_box_id();
        void release_box_id(int box_id);

        #ifdef _WIN32
        // Windows: CreateProcess + Job Object
        RunResult launch_windows(
            const std::string& runner_exe,
            const std::string& args,
            const std::filesystem::path& working_dir,
            const JobConfig& job_config
        );
        #else
        // Linux: isolate --init, --run, --cleanup
        RunResult launch_isolate(
            int box_id,
            const std::string& runner_exe,
            const std::filesystem::path& input_dir,
            const std::filesystem::path& output_dir,
            int thread_count,
            const std::string& monitor_mode,
            const JobConfig& job_config
        );

        // Parse isolate meta-file: time, time-wall, max-rss, cg-mem, cg-oom-killed, exitcode, status
        static RunResult parse_meta_file(const std::filesystem::path& meta_path);
        #endif
};
