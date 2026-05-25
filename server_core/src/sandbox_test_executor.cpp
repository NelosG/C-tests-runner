#include "sandbox_test_executor.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <log_utils.h>
#include <map>
#include <path_sanitizer.h>
#include <scope_cleanup.h>
#include <test_data.h>
#include <test_registry.h>
#include <time_utils.h>

namespace fs = std::filesystem;


namespace {

    struct TestInput {
        fs::path dir;        ///< per-test input directory (bind-mounted into sandbox)
        TestData data;       ///< the populated map, retained for verify()
    };

    /// Run setup(input), serialize to input_dir/input.bin. Returns nullopt on
    /// failure (and pushes a failed TestResult into `test_results`).
    std::optional<TestInput> setup_test_input(
        const Test& test,
        const std::string& key,
        const fs::path& temp_base,
        const std::string& scenario_name,
        std::vector<TestResult>& test_results
    ) {
        fs::path input_dir = temp_base / ("input-" + scenario_name + "-" + test.name);
        fs::create_directories(input_dir);
        try {
            TestInput out;
            out.dir = input_dir;
            if(test.has_raw_input()) {
                // Pre-built TLV blob (e.g. converted from a pbbs
                // testInput); skip setup() and copy the file straight
                // in as the sandbox-side input.bin.
                std::error_code ec;
                fs::copy_file(test.raw_input_path,
                              input_dir / "input.bin",
                              fs::copy_options::overwrite_existing,
                              ec);
                if(ec) throw std::runtime_error(
                    "Failed to copy raw_input_path '" + test.raw_input_path
                    + "': " + ec.message());
                // Mirror the bytes into out.data so verify() can still
                // call input.read_* (matches the non-raw code path).
                out.data = TestData::load(input_dir / "input.bin");
            } else {
                test.setup(out.data);
                out.data.save(input_dir / "input.bin");
            }
            return out;
        } catch(const std::exception& e) {
            std::error_code ec;
            fs::remove_all(input_dir, ec);
            LOG_ERR("SandboxTestExecutor") << "Setup failed for " << key
                << ": " << e.what() << "\n";
            test_results.emplace_back(
                test.name,
                false,
                std::string("Setup failed: ") + e.what(),
                0.0
            );
            return std::nullopt;
        }
    }

    SandboxLauncher::JobConfig make_job_config(
        int thread_count,
        long long memory_limit_kb,
        int wall_time_sec,
        int cpu_time_sec,
        const std::string& cpus,
        const std::vector<std::string>& extra_lib_dirs,
        int max_processes
    ) {
        SandboxLauncher::JobConfig cfg;
        cfg.wall_time_sec = wall_time_sec;
        cfg.cpu_time_sec = cpu_time_sec;
        cfg.memory_limit_kb = static_cast<int>(std::min(
            memory_limit_kb,
            static_cast<long long>(std::numeric_limits<int>::max())
        ));
        cfg.max_processes = max_processes > 0 ? max_processes : thread_count * 2;
        cfg.cpus = cpus;
        cfg.extra_lib_dirs = extra_lib_dirs;
        return cfg;
    }

} // namespace

// ============================================================================
// Construction
// ============================================================================

SandboxTestExecutor::SandboxTestExecutor(SandboxLauncher& sandbox, CpuIsolator& cpu_isolator)
    : sandbox_(sandbox), cpu_isolator_(cpu_isolator) {}

// ============================================================================
// run() - outer scenario/test loop
// ============================================================================

std::vector<TestScenarioResult> SandboxTestExecutor::run(
    const std::string& runner_exe,
    const TestRegistry& registry,
    ScenarioType type_filter,
    const std::vector<int>& thread_counts,
    const std::string& monitor_mode,
    long long memory_limit_kb,
    const std::string& job_id,
    int wall_time_sec,
    int cpu_time_sec,
    progress::callback on_progress,
    const std::string& node_id,
    const std::vector<std::string>& extra_lib_dirs,
    int max_processes
) {
    // Per-test progress emitter - no-op when on_progress isn't set (CLI / older callers).
    auto emit_test = [&](
        const std::string& scenario,
        const std::string& test,
        int tc,
        const std::string& status,
        std::optional<double> time_ms,
        const std::string& message
    ) {
        if(!on_progress) return;
        nlohmann::json event = {
            {"jobId", job_id},
            {"nodeId", node_id},
            {"phase", "test"},
            {"scenario", scenario},
            {"test", test},
            {"threadCount", tc},
            {"status", status},
            {"timestamp", now_iso8601()}
        };
        if(time_ms.has_value()) event["timeMs"] = *time_ms;
        if(!message.empty()) event["message"] = message;
        try { on_progress(event); } catch(...) {}
    };
    std::vector<TestScenarioResult> all_results;

    // Setup once per test; input is reused across thread counts.
    std::map<std::string, TestInput> inputs;

    fs::path temp_base = fs::temp_directory_path() / ("ctr-sandbox-" + job_id);
    fs::create_directories(temp_base);

    for(int tc : thread_counts) {
        for(const auto& scenario : registry.all()) {
            if(scenario->scenario_type() != type_filter) continue;

            auto tests = scenario->get_tests();
            std::vector<TestResult> test_results;
            test_results.reserve(tests.size());

            for(const auto& test : tests) {
                std::string key = scenario->name() + "::" + test.name;

                if(inputs.find(key) == inputs.end()) {
                    auto built = setup_test_input(
                        test,
                        key,
                        temp_base,
                        scenario->name(),
                        test_results
                    );
                    if(!built) continue;
                    inputs[key] = std::move(*built);
                }

                const auto& input = inputs[key];
                const auto& input_dir = input.dir;
                fs::path output_dir = temp_base / ("output-" + scenario->name()
                    + "-" + test.name + "-t" + std::to_string(tc));
                fs::create_directories(output_dir);

                std::string allocated_cpus;
                try {
                    allocated_cpus = cpu_isolator_.allocate(tc);
                } catch(const std::exception& e) {
                    LOG_ERR("SandboxTestExecutor") << "CPU allocation failed: " << e.what() << "\n";
                }
                ScopeCleanup cpu_guard{
                    [&]() {
                        if(!allocated_cpus.empty()) cpu_isolator_.release(allocated_cpus);
                    }
                };

                auto job_config = make_job_config(
                    tc,
                    memory_limit_kb,
                    wall_time_sec,
                    cpu_time_sec,
                    allocated_cpus,
                    extra_lib_dirs,
                    max_processes
                );

                emit_test(scenario->name(), test.name, tc, "running", std::nullopt, "");

                auto [run_result, timing_json] = sandbox_.execute(
                    runner_exe,
                    input_dir,
                    output_dir,
                    tc,
                    monitor_mode,
                    job_config
                );

                // Surface non-zero / sandbox-side failures in the engine log so
                // operators can see what went wrong without grepping per-test
                // stderrOutput in the JSON result.
                if(run_result.timed_out || run_result.oom_killed
                    || run_result.exit_code != 0) {
                    LOG_ERR("SandboxTestExecutor")
                        << scenario->name() << "::" << test.name
                        << " (t=" << tc << ") sandbox returned "
                        << (run_result.timed_out
                            ? "timeout"
                            : run_result.oom_killed
                            ? "oom"
                            : "exit=" + std::to_string(run_result.exit_code))
                        << (run_result.stderr_output.empty()
                            ? std::string{}
                            : "; stderr=" + run_result.stderr_output)
                        << "\n";
                }

                auto built = build_test_result(
                    test,
                    run_result,
                    timing_json,
                    input.data,
                    output_dir
                );
                emit_test(
                    scenario->name(),
                    test.name,
                    tc,
                    built.passed ? "passed" : "failed",
                    built.time_ms,
                    built.passed ? "" : built.message
                );
                test_results.push_back(std::move(built));

                std::error_code ec;
                fs::remove_all(output_dir, ec);
            }

            all_results.emplace_back(scenario->name(), std::move(test_results), tc);
        }
    }

    // Recursive remove: handles partial cleanup (e.g. Windows file locks
    // leaving subdirs un-deletable). Idempotent - empty / partially-empty
    // / fully-populated states all collapse to "gone".
    {
        std::error_code ec;
        fs::remove_all(temp_base, ec);
    }

    return all_results;
}

// build_test_result + build_summary live in test_result_builder.cpp - they're
// pure post-processing helpers (TestResult building, scalability aggregation)
// independent of the per-test sandbox loop above.
