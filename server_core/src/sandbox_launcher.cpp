#include "sandbox_launcher.h"
#include "process_utils.h"
#include "log_utils.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Common
// ---------------------------------------------------------------------------

SandboxLauncher::SandboxLauncher(Config config)
    : config_(std::move(config)) {
    for(int i = 0; i < config_.max_box_id; ++i) {
        available_boxes_.insert(i);
    }
}

int SandboxLauncher::acquire_box_id() {
    std::lock_guard<std::mutex> lock(box_mutex_);
    if(available_boxes_.empty()) return -1;
    auto it = available_boxes_.begin();
    int id = *it;
    available_boxes_.erase(it);
    return id;
}

void SandboxLauncher::release_box_id(int box_id) {
    std::lock_guard<std::mutex> lock(box_mutex_);
    available_boxes_.insert(box_id);
}

std::pair<SandboxLauncher::RunResult, std::optional<nlohmann::json>>
    SandboxLauncher::execute(
        const std::string& runner_exe_path,
        const fs::path& input_dir,
        const fs::path& output_dir,
        int thread_count,
        const std::string& monitor_mode,
        const JobConfig& job_config
    ) {
    // Validate monitor_mode - only allow known values to prevent injection
    std::string safe_mode = "normal";
    if(monitor_mode == "stress" || monitor_mode == "monitor" || monitor_mode == "normal") {
        safe_mode = monitor_mode;
    } else {
        LOG_ERR("SandboxLauncher") << "Unknown monitor-mode '" << monitor_mode
            << "', defaulting to 'normal'\n";
    }

    // Build runner argument string (shell-quoted to prevent injection)
    std::string args = "--input " + shell_quote(input_dir.string())
        + " --output " + shell_quote(output_dir.string())
        + " --threads " + std::to_string(thread_count)
        + " --monitor-mode " + safe_mode;

    RunResult result;

    #ifdef _WIN32
    result = launch_windows(runner_exe_path, args, output_dir.parent_path(), job_config);
    #else
    int box_id = acquire_box_id();
    if(box_id < 0) {
        result.stderr_output = "No sandbox box IDs available";
        LOG_ERR("SandboxLauncher") << "No sandbox box IDs available (all "
            << config_.max_box_id << " in use)\n";
        return {result, std::nullopt};
    }
    try {
        result = launch_isolate(
            box_id,
            runner_exe_path,
            input_dir,
            output_dir,
            thread_count,
            safe_mode,
            job_config
        );
    } catch(...) {
        release_box_id(box_id);
        throw;
    }
    release_box_id(box_id);
    #endif

    // Try to read meta.bin from output_dir (binary runner metadata)
    std::optional<nlohmann::json> output_json;
    fs::path meta_path = output_dir / "meta.bin";
    if(fs::exists(meta_path)) {
        try {
            std::ifstream f(meta_path, std::ios::binary);
            // Meta struct: 8 bytes timeMs + 19*8 bytes parallel stats = 160 bytes
            constexpr size_t META_SIZE = 8 + 19 * 8; // 160 bytes
            char buf[META_SIZE] = {};
            f.read(buf, META_SIZE);
            if(f.gcount() != META_SIZE) {
                LOG_ERR("SandboxLauncher") << "meta.bin truncated ("
                    << f.gcount() << "/" << META_SIZE << " bytes)\n";
                // Don't set output_json - treat as missing
            } else {
                double time_ms = 0.0;
                int64_t stats[19] = {};
                std::memcpy(&time_ms, buf, 8);
                std::memcpy(stats, buf + 8, 19 * 8);

                nlohmann::json j;
                j["timeMs"] = time_ms;
                j["parallelStats"] = {
                    {"parallelRegions", static_cast<int>(stats[0])},
                    {"tasksCreated", static_cast<int>(stats[1])},
                    {"maxThreadsUsed", static_cast<int>(stats[2])},
                    {"singleRegions", static_cast<int>(stats[3])},
                    {"taskWaits", static_cast<int>(stats[4])},
                    {"barriers", static_cast<int>(stats[5])},
                    {"criticals", static_cast<int>(stats[6])},
                    {"forLoops", static_cast<int>(stats[7])},
                    {"atomics", static_cast<int>(stats[8])},
                    {"sections", static_cast<int>(stats[9])},
                    {"masters", static_cast<int>(stats[10])},
                    {"ordered", static_cast<int>(stats[11])},
                    {"taskGroups", static_cast<int>(stats[12])},
                    {"simdConstructs", static_cast<int>(stats[13])},
                    {"cancels", static_cast<int>(stats[14])},
                    {"flushes", static_cast<int>(stats[15])},
                    {"taskYields", static_cast<int>(stats[16])},
                    {"workNs", stats[17]},
                    {"spanNs", stats[18]}
                };
                output_json = std::move(j);
            } // else (META_SIZE bytes read)
        } catch(const std::exception& e) {
            LOG_ERR("SandboxLauncher") << "Failed to read meta.bin: " << e.what() << "\n";
        }
    }

    return {result, output_json};
}

// ---------------------------------------------------------------------------
// Windows implementation
// ---------------------------------------------------------------------------

#ifdef _WIN32

static ULONG_PTR parse_affinity_mask(const std::string& cpus) {
    ULONG_PTR mask = 0;
    if(cpus.empty()) return 0;

    constexpr int max_core = sizeof(ULONG_PTR) * 8 - 1;

    // Parse formats: "4-7" (range) or "1,3,5" (list) or "4-7,10"
    std::istringstream ss(cpus);
    std::string token;
    while(std::getline(ss, token, ',')) {
        auto dash = token.find('-');
        if(dash != std::string::npos) {
            int lo = std::stoi(token.substr(0, dash));
            int hi = std::stoi(token.substr(dash + 1));
            for(int i = lo; i <= hi; ++i) {
                if(i < 0 || i > max_core) continue;
                mask |= (static_cast<ULONG_PTR>(1) << i);
            }
        } else {
            int core = std::stoi(token);
            if(core >= 0 && core <= max_core) {
                mask |= (static_cast<ULONG_PTR>(1) << core);
            }
        }
    }
    return mask;
}

SandboxLauncher::RunResult SandboxLauncher::launch_windows(
    const std::string& runner_exe,
    const std::string& args,
    const fs::path& working_dir,
    const JobConfig& job_config
) {
    RunResult result;

    // --- Create Job Object with resource limits ---
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if(!job) {
        DWORD err = GetLastError();
        result.stderr_output = "Failed to create Job Object: " + std::to_string(err);
        LOG_ERR("SandboxLauncher") << "CreateJobObjectA failed (GetLastError=" << err << ")\n";
        return result;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits = {};
    job_limits.BasicLimitInformation.LimitFlags = 0;

    // Memory limit
    if(job_config.memory_limit_kb > 0) {
        job_limits.ProcessMemoryLimit = static_cast<SIZE_T>(job_config.memory_limit_kb) * 1024;
        job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    }

    // CPU time limit (per-process, in 100ns units)
    if(job_config.cpu_time_sec > 0) {
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(job_config.cpu_time_sec) * 10000000LL;
        job_limits.BasicLimitInformation.PerProcessUserTimeLimit = li;
        job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
    }

    // CPU affinity
    if(!job_config.cpus.empty()) {
        ULONG_PTR mask = parse_affinity_mask(job_config.cpus);
        if(mask != 0) {
            job_limits.BasicLimitInformation.Affinity = mask;
            job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_AFFINITY;
        }
    }

    SetInformationJobObject(
        job,
        JobObjectExtendedLimitInformation,
        &job_limits,
        sizeof(job_limits)
    );

    // --- Create pipes for stderr capture ---
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;
    if(!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        DWORD err = GetLastError();
        result.stderr_output = "Failed to create stderr pipe: " + std::to_string(err);
        LOG_ERR("SandboxLauncher") << "CreatePipe(stderr) failed (GetLastError=" << err << ")\n";
        CloseHandle(job);
        return result;
    }
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // --- Redirect stdin/stdout to NUL (runner should not access server I/O) ---
    HANDLE h_nul = CreateFileA(
        "NUL",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        0,
        nullptr
    );

    // --- Launch process suspended ---
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdError = stderr_write;
    si.hStdInput = h_nul;
    si.hStdOutput = h_nul;

    PROCESS_INFORMATION pi = {};
    std::string cmd_line = "\"" + runner_exe + "\" " + args;
    std::string work_dir = working_dir.string();

    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(cmd_line.c_str()),
        nullptr,
        nullptr,
        TRUE,  // inherit handles
        CREATE_SUSPENDED,
        nullptr,
        work_dir.c_str(),
        &si,
        &pi
    );

    // Close the write end of stderr pipe in parent
    CloseHandle(stderr_write);
    stderr_write = nullptr;

    if(!created) {
        DWORD err = GetLastError();
        result.stderr_output = "CreateProcess failed: " + std::to_string(err);
        LOG_ERR("SandboxLauncher") << "CreateProcessA failed for '" << runner_exe
            << "' (GetLastError=" << err << ")\n";
        CloseHandle(stderr_read);
        if(h_nul != INVALID_HANDLE_VALUE) CloseHandle(h_nul);
        CloseHandle(job);
        return result;
    }

    // --- Assign to job and resume ---
    AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);

    // --- Read stderr in a background thread to prevent pipe buffer deadlock ---
    std::string stderr_output;
    // Cap captured stderr at 1 MiB. A misbehaving runner that prints
    // gigabytes of errors before timing out would otherwise OOM the server.
    constexpr size_t kStderrCapBytes = 1 * 1024 * 1024;
    std::thread stderr_reader(
        [&stderr_output, stderr_read]() {
            char buf[4096];
            DWORD bytes_read = 0;
            bool truncated = false;
            while(ReadFile(stderr_read, buf, sizeof(buf), &bytes_read, nullptr) && bytes_read > 0) {
                if(stderr_output.size() < kStderrCapBytes) {
                    size_t room = kStderrCapBytes - stderr_output.size();
                    stderr_output.append(buf, std::min<size_t>(bytes_read, room));
                    if(stderr_output.size() == kStderrCapBytes && !truncated) {
                        stderr_output.append("\n[...stderr truncated at 1 MiB...]\n");
                        truncated = true;
                    }
                }
                // Continue draining the pipe even after truncation, otherwise the
                // child blocks on a full pipe and never exits.
            }
        }
    );

    // RAII: ensure stderr_reader is joined even if an exception is thrown
    struct ThreadJoiner {
        std::thread& t;
        ~ThreadJoiner() { if(t.joinable()) t.join(); }
    } stderr_joiner{stderr_reader};

    // --- Wait with wall-time timeout ---
    DWORD timeout_ms = (job_config.wall_time_sec > 0)
        ? static_cast<DWORD>(job_config.wall_time_sec) * 1000
        : INFINITE;

    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);
    if(wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);  // wait for termination
        result.timed_out = true;
    }

    // Join the reader thread BEFORE reading stderr_output - otherwise we'd
    // race with the still-draining ReadFile loop. The RAII joiner is kept as
    // an exception-safety fallback (no-op once we've joined explicitly).
    if(stderr_reader.joinable()) stderr_reader.join();
    result.stderr_output = std::move(stderr_output);

    // --- Collect exit code ---
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    // --- Collect CPU time ---
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if(GetProcessTimes(pi.hProcess, &creation_time, &exit_time, &kernel_time, &user_time)) {
        ULARGE_INTEGER user;
        user.LowPart = user_time.dwLowDateTime;
        user.HighPart = user_time.dwHighDateTime;
        result.cpu_time_sec = static_cast<double>(user.QuadPart) / 10000000.0;  // 100ns -> sec
    }

    // --- Collect wall time from process creation/exit ---
    {
        ULARGE_INTEGER start, end;
        start.LowPart = creation_time.dwLowDateTime;
        start.HighPart = creation_time.dwHighDateTime;
        end.LowPart = exit_time.dwLowDateTime;
        end.HighPart = exit_time.dwHighDateTime;
        if(end.QuadPart > start.QuadPart) {
            result.wall_time_sec = static_cast<double>(end.QuadPart - start.QuadPart) / 10000000.0;
        }
    }

    // --- Collect peak memory ---
    PROCESS_MEMORY_COUNTERS pmc = {};
    pmc.cb = sizeof(pmc);
    if(GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
        result.max_rss_kb = static_cast<long long>(pmc.PeakWorkingSetSize) / 1024;
        result.cg_mem_peak_kb = result.max_rss_kb;  // Windows: use same value
    }

    // --- Check if OOM killed (exit code from Job memory limit) ---
    if(exit_code != 0 && !result.timed_out) {
        // Windows doesn't set a specific exit code for OOM, but the process
        // gets terminated when exceeding job memory limit.
        // We can check if peak memory is near the limit.
        if(job_config.memory_limit_kb > 0 &&
            result.max_rss_kb >= job_config.memory_limit_kb * 95 / 100) {
            result.oom_killed = true;
        }
    }

    // --- Cleanup handles ---
    CloseHandle(stderr_read);
    if(h_nul != INVALID_HANDLE_VALUE) CloseHandle(h_nul);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(job);

    return result;
}

#else

// ---------------------------------------------------------------------------
// Linux (isolate) implementation
// ---------------------------------------------------------------------------

SandboxLauncher::RunResult SandboxLauncher::launch_isolate(
    int box_id,
    const std::string& runner_exe,
    const fs::path& input_dir,
    const fs::path& output_dir,
    int thread_count,
    const std::string& monitor_mode,
    const JobConfig& job_config
) {
    RunResult result;
    std::string box_id_str = std::to_string(box_id);
    fs::path meta_path = "/tmp/isolate-meta-" + box_id_str + ".txt";

    // 1. Initialize sandbox box
    std::string init_cmd = shell_quote(config_.isolate_path) + " --init --box-id=" + box_id_str + " --cg 2>&1";
    auto init_result = run_command(init_cmd);
    if(init_result.failed()) {
        result.stderr_output = "isolate --init failed: " + init_result.output;
        LOG_ERR("SandboxLauncher") << "isolate --init failed (box-id=" << box_id_str
            << ", exit=" << init_result.exit_code
            << ", path=" << config_.isolate_path << "): "
            << init_result.output;
        return result;
    }

    // 2. Ensure output directory exists (bind-mounted rw into sandbox)
    //    Must be world-writable: isolate runs as box user (uid 60000+box_id)
    fs::create_directories(output_dir);
    fs::permissions(output_dir, fs::perms::all, fs::perm_options::replace);

    // 3. Build isolate --run command with bind mounts
    // Runner dir contains the runner exe + student .so libs (read-only mount)
    // Input dir contains test data (read-only mount)
    // Output dir receives results including output.json (read-write mount)
    // System libs (libc, libstdc++, libgomp) available via isolate's default /lib, /lib64, /usr mounts
    fs::path runner_dir = fs::path(runner_exe).parent_path();
    std::string runner_filename = fs::path(runner_exe).filename().string();

    std::ostringstream run_cmd;
    if(!job_config.cpus.empty()) {
        run_cmd << "taskset -c " << job_config.cpus << " ";
    }
    // Bind-mount paths inside the isolate sandbox - these are paths the
    // sandboxed runner sees, not host paths. Hardcoded but referenced in
    // multiple places below (mount args + LD_LIBRARY_PATH + final exec line).
    static constexpr const char* kSandboxRunnerDir = "/runner";
    static constexpr const char* kSandboxInputDir = "/input";
    static constexpr const char* kSandboxOutputDir = "/output";

    run_cmd << shell_quote(config_.isolate_path) << " --run --box-id=" << box_id_str << " --cg";

    run_cmd << " --dir=" << kSandboxRunnerDir << "=" << shell_quote(runner_dir.string());
    run_cmd << " --dir=" << kSandboxInputDir << "=" << shell_quote(input_dir.string());
    run_cmd << " --dir=" << kSandboxOutputDir << "=" << shell_quote(output_dir.string()) << ":rw";

    if(job_config.memory_limit_kb > 0) {
        run_cmd << " --cg-mem=" << job_config.memory_limit_kb;
    }
    if(job_config.cpu_time_sec > 0) {
        run_cmd << " --time=" << job_config.cpu_time_sec;
    }
    if(job_config.wall_time_sec > 0) {
        run_cmd << " --wall-time=" << job_config.wall_time_sec;
    }
    if(job_config.max_processes > 0) {
        run_cmd << " --processes=" << job_config.max_processes;
    }
    run_cmd << " --meta=" << shell_quote(meta_path.string());
    run_cmd << " --env=OMP_NUM_THREADS=" << thread_count;
    // Keep OMP threads spinning between parallel regions (instant dispatch
    // instead of kernel wakeup) and pin them to neighbouring cores. Harmless
    // for non-OMP runners - Parlay/Cilk/Seq ignore these env vars.
    run_cmd << " --env=OMP_WAIT_POLICY=active";
    run_cmd << " --env=OMP_PROC_BIND=close";

    // Bind-mount any extra lib dirs (e.g. OpenCilk runtime) and extend
    // LD_LIBRARY_PATH so dlopen/ld.so can resolve their .so files.
    std::string ld_library_path = kSandboxRunnerDir;
    for(size_t i = 0; i < job_config.extra_lib_dirs.size(); ++i) {
        std::string mount = "/extra-libs-" + std::to_string(i);
        run_cmd << " --dir=" << mount << "=" << shell_quote(job_config.extra_lib_dirs[i]);
        ld_library_path += ":" + mount;
    }
    run_cmd << " --env=LD_LIBRARY_PATH=" << ld_library_path;

    run_cmd << " -- " << kSandboxRunnerDir << "/" << runner_filename
        << " --input " << kSandboxInputDir
        << " --output " << kSandboxOutputDir
        << " --threads " << thread_count
        << " --monitor-mode " << monitor_mode;

    run_cmd << " 2>&1";

    // 4. Execute via popen
    auto exec_result = run_command(run_cmd.str());
    result.stderr_output = exec_result.output;

    // 5. Parse meta-file for resource usage and exit status
    if(fs::exists(meta_path)) {
        result = parse_meta_file(meta_path);
        // Preserve stderr from exec
        result.stderr_output = exec_result.output;
        // Clean up meta file
        std::error_code ec;
        fs::remove(meta_path, ec);
    } else {
        result.exit_code = exec_result.exit_code;
    }

    // 9. Cleanup sandbox
    std::string cleanup_cmd = shell_quote(config_.isolate_path) + " --cleanup --box-id=" + box_id_str + " --cg 2>&1";
    auto cleanup_result = run_command(cleanup_cmd);
    if(cleanup_result.failed()) {
        LOG_ERR("SandboxLauncher") << "isolate --cleanup failed (box-id=" << box_id_str
            << ", exit=" << cleanup_result.exit_code << "): "
            << cleanup_result.output;
    }

    return result;
}

SandboxLauncher::RunResult SandboxLauncher::parse_meta_file(const fs::path& meta_path) {
    RunResult result;
    std::ifstream f(meta_path);
    if(!f.is_open()) return result;

    // Each numeric field is parsed defensively: a corrupted meta line (partial
    // write on OOM, kernel hiccup mid-write) would otherwise throw
    // std::invalid_argument / std::out_of_range from stod/stoi/stoll, the
    // exception would propagate up through execute() -> SandboxTestExecutor::run
    // and kill the worker without producing a clean result.
    std::string line;
    while(std::getline(f, line)) {
        auto colon = line.find(':');
        if(colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        try {
            if(key == "time") {
                result.cpu_time_sec = std::stod(value);
            } else if(key == "time-wall") {
                result.wall_time_sec = std::stod(value);
            } else if(key == "max-rss") {
                result.max_rss_kb = std::stoll(value);
            } else if(key == "cg-mem") {
                result.cg_mem_peak_kb = std::stoll(value);
            } else if(key == "cg-oom-killed") {
                result.oom_killed = (value == "1");
            } else if(key == "exitcode") {
                result.exit_code = std::stoi(value);
            } else if(key == "exitsig") {
                result.exit_code = -std::stoi(value);
            } else if(key == "status") {
                if(value == "TO") {
                    result.timed_out = true;
                }
            }
        } catch(const std::exception& e) {
            LOG_ERR("SandboxLauncher") << "Malformed meta-file line ('"
                << key << "'): " << e.what() << " - skipping field\n";
        }
    }

    return result;
}

#endif
