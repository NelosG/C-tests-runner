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
        /// Number of untimed RUNNER_EXECUTE iterations before the timed one.
        /// 0 = single timed pass (default). >0 warms page caches / scheduler
        /// state so the timed pass excludes first-touch overhead, matching
        /// pbbs's time_loop semantics. Only safe for wrappers that do NOT
        /// mutate their input arguments.
        int warmup_iterations = 0;
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

    /// Convenience wrappers - `runner::read_*<T>("key")`. Equivalent to
    /// `input().read_*<T>(key)` followed by `input().erase(key)`. The erase
    /// frees the raw bytes for `key` after parsing, halving peak memory
    /// inside the sandboxed runner for large arrays (typical case: 80 MB
    /// raw input + 80 MB parsed vector -> keep only the parsed copy).
    /// Single-shot: a second call for the same key throws.
    template<typename T>
    T read_value(const std::string& k) {
        T v = input().read_value<T>(k);
        input().erase(k);
        return v;
    }

    template<typename T>
    std::vector<T> read_array(const std::string& k) {
        std::vector<T> v = input().read_array<T>(k);
        input().erase(k);
        return v;
    }

    inline std::string read_string(const std::string& k) {
        std::string v = input().read_string(k);
        input().erase(k);
        return v;
    }

    inline std::vector<std::string> read_strings(const std::string& k) {
        std::vector<std::string> v = input().read_strings(k);
        input().erase(k);
        return v;
    }

    /// Read a composite object written via write_object. Returns the parsed
    /// sub-TestData by value; the raw bytes are erased from input() like the
    /// other consumer-style readers.
    inline TestData read_object(const std::string& k) {
        TestData v = input().read_object(k);
        input().erase(k);
        return v;
    }

    template<typename T>
    void write_value(const std::string& k, T v) { output().write_value<T>(k, v); }

    template<typename T>
    void write_array(const std::string& k, const std::vector<T>& v) { output().write_array<T>(k, v); }

    inline void write_string(const std::string& k, const std::string& s) { output().write_string(k, s); }
    inline void write_strings(const std::string& k, const std::vector<std::string>& v) { output().write_strings(k, v); }

    template<typename F>
    void write_object(const std::string& k, F&& builder) {
        output().write_object(k, std::forward<F>(builder));
    }

    // ========================================================================
    // Timing
    // ========================================================================

    void begin_execute();
    void end_execute();
    double execute_time_ms();

    /// Loop driver for RUNNER_EXECUTE. Walks `warmup_iterations` untimed
    /// runs followed by exactly one timed run, then returns false to exit
    /// the loop. Internal - prefer the RUNNER_EXECUTE macro below.
    bool _execute_should_continue();

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

/// Runs the test body `warmup_iterations + 1` times. The first
/// `warmup_iterations` runs are untimed (their effect on runner::output()
/// is discarded - the output map is cleared between iterations). The last
/// run is the timed one; its wall time is reported.
///
/// With warmup_iterations = 0 (default) this is exactly one timed pass,
/// matching the original semantics.
///
/// IMPORTANT: enabling warmup is only safe if the body does NOT mutate its
/// input. In-place sorts etc. must keep warmup_iterations = 0.
#define RUNNER_EXECUTE \
    while (runner::_execute_should_continue())
