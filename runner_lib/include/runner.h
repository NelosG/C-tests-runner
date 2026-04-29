#pragma once

/**
 * @file runner.h
 * @brief Runner library API for student test executables.
 *
 * The runner reads input.bin (a TestData TLV file) into runner::input(),
 * runs the user's RUNNER_EXECUTE block (timed), then writes runner::output()
 * to output.bin. Per-key typed access via TestData::read_*<T> / write_*<T>.
 */

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <test_data.h>
#include <vector>

namespace runner {

    // ========================================================================
    // Meta output (parallel-construct stats written by RUNNER_MAIN -> finish())
    // ========================================================================

    struct Meta {
        double time_ms = 0.0;
        // Parallel stats (17 int counters stored as int64 + 2 long long)
        int64_t parallel_regions = 0;
        int64_t tasks_created = 0;
        int64_t max_threads_used = 0;
        int64_t single_regions = 0;
        int64_t taskwaits = 0;
        int64_t barriers = 0;
        int64_t criticals = 0;
        int64_t for_loops = 0;
        int64_t atomics = 0;
        int64_t sections = 0;
        int64_t masters = 0;
        int64_t ordered = 0;
        int64_t taskgroups = 0;
        int64_t simd_constructs = 0;
        int64_t cancels = 0;
        int64_t flushes = 0;
        int64_t taskyields = 0;
        int64_t work_ns = 0;
        int64_t span_ns = 0;
    };
    static_assert(sizeof(Meta) == 8 + 19 * 8, "Meta must be 160 bytes");

    // ========================================================================
    // Config & lifecycle
    // ========================================================================

    struct RunnerConfig {
        std::string input_dir;
        std::string output_dir;
        int thread_count = 1;
        std::string monitor_mode = "normal";
    };

    const RunnerConfig& init(int argc, char* argv[]);
    const RunnerConfig& config();

    /// Framework-specific runtime initialisation. Implemented by exactly one of
    /// runner_omp / runner_parlay / runner_cilk / runner_seq static libs (the
    /// linker picks whichever is linked in). Called automatically by RUNNER_MAIN
    /// after init() and before the user body runs.
    void setup();

    // ========================================================================
    // Input / output map access
    // ========================================================================

    /// Mutable access to the input TestData (loaded from input_dir/input.bin).
    /// Read-only in user code; loaded automatically by RUNNER_MAIN.
    TestData& input();

    /// Output TestData built up during the test; flushed to
    /// output_dir/output.bin by RUNNER_MAIN.
    TestData& output();

    /// Convenience wrappers - equivalent to `input().read_*<T>(...)` /
    /// `output().write_*<T>(...)`. Provided for short student-facing code.
    template<typename T> T read_value(const std::string& k) { return input().read_value<T>(k); }
    template<typename T> std::vector<T> read_array(const std::string& k) { return input().read_array<T>(k); }
    inline std::string read_string(const std::string& k) { return input().read_string(k); }
    inline std::vector<std::string> read_strings(const std::string& k) { return input().read_strings(k); }

    template<typename T> void write_value(const std::string& k, T v) { output().write_value<T>(k, v); }
    template<typename T> void write_array(const std::string& k, const std::vector<T>& v) { output().write_array<T>(k, v); }
    inline void write_string(const std::string& k, const std::string& s) { output().write_string(k, s); }
    inline void write_strings(const std::string& k, const std::vector<std::string>& v) { output().write_strings(k, v); }

    // ========================================================================
    // Timing
    // ========================================================================

    void begin_execute();
    void end_execute();
    double execute_time_ms();

    /// Advances the RUNNER_EXECUTE pass counter and bumps the timer. Called
    /// from the `iter` clause of the for-loop the macro expands to.
    ///   pass=0 -> end of warmup iteration -> start timing
    ///   pass=1 -> end of timed iteration -> stop timing
    void next_pass(int& pass);

    // ========================================================================
    // Finish hook (framework variants inject parallelStats into Meta)
    // ========================================================================

    using finish_hook_t = std::function<void(Meta&)>;
    void set_finish_hook(finish_hook_t hook);

    /// Load input.bin from input_dir (called automatically by RUNNER_MAIN).
    void load_input();
    /// Write output.bin and meta.bin to output_dir.
    void finish();

} // namespace runner

#define RUNNER_MAIN \
    static void runner_user_code_(); \
    int main(int argc, char* argv[]) { \
        runner::init(argc, argv); \
        runner::setup(); \
        runner::load_input(); \
        runner_user_code_(); \
        runner::finish(); \
        return 0; \
    } \
    static void runner_user_code_()

/// Runs the test body twice: a warmup pass (untimed - primes caches, the OMP
/// thread pool, page faults), then the real timed pass. Without this, micro
/// benchmarks see the cost of first-touch and pool spin-up baked into T1,
/// which makes Tp look "super-linear" (e.g. 13x speedup on 4 threads). The
/// student body must be deterministic and idempotent - re-executing should
/// produce the same output.
#define RUNNER_EXECUTE \
    for (int _runner_pass_ = 0; _runner_pass_ < 2; runner::next_pass(_runner_pass_))
