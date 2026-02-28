#include <iostream>
#include <test_result_converter.h>
#include <test_scenario_result_converter.h>

nlohmann::json TestScenarioResultConverter::to_grouped_json(
    const std::vector<TestScenarioResult>& results,
    const std::vector<int>& thread_counts,
    bool perf_mode,
    int numa_node
) {
    if(results.empty() || thread_counts.empty()) {
        return nlohmann::json::array();
    }

    if(results.size() % thread_counts.size() != 0) {
        std::cerr << "[TestScenarioResultConverter] Warning: results.size() ("
            << results.size() << ") is not divisible by thread_counts.size() ("
            << thread_counts.size() << ") — returning empty array\n";
        return nlohmann::json::array();
    }

    size_t num_scenarios = results.size() / thread_counts.size();
    auto scenarios_json = nlohmann::json::array();

    for(size_t s = 0; s < num_scenarios; ++s) {
        nlohmann::json scenario;
        scenario["name"] = results[s].name;

        if(numa_node >= 0) {
            scenario["numaNode"] = numa_node;
        }

        // Group individual tests: test -> runs across thread counts
        size_t num_tests = results[s].results.size();
        auto tests_grouped = nlohmann::json::array();

        for(size_t ti = 0; ti < num_tests; ++ti) {
            nlohmann::json test_entry;
            test_entry["name"] = results[s].results[ti].name;

            // Baseline time from thread_counts[0] (caller must pass 1 as first element)
            double t1_ms = results[s].results[ti].time_ms;

            auto runs = nlohmann::json::array();
            for(size_t t = 0; t < thread_counts.size(); ++t) {
                const auto& r = results[t * num_scenarios + s];
                const auto& tr = r.results[ti];

                nlohmann::json run;
                run["threads"] = r.threads;
                run["passed"] = tr.passed;
                if(!tr.message.empty()) {
                    run["message"] = tr.message;
                }

                // Stats sub-object: timing + performance metrics
                double work_ms = static_cast<double>(tr.work_ns) / 1e6;
                double span_ms = static_cast<double>(tr.span_ns) / 1e6;
                double parallelism = (tr.span_ns > 0)
                    ? static_cast<double>(tr.work_ns) / static_cast<double>(tr.span_ns)
                    : 0.0;
                double speedup = (tr.time_ms > 0.0) ? t1_ms / tr.time_ms : 0.0;
                double efficiency = (r.threads > 0) ? speedup / r.threads : 0.0;

                run["stats"] = {
                    {"timeMs", tr.time_ms},
                    {"workMs", work_ms},
                    {"spanMs", span_ms},
                    {"parallelism", parallelism},
                    {"speedup", speedup},
                    {"efficiency", efficiency}
                };

                // Parallel stats sub-object: construct usage
                run["parallelStats"] = {
                    {"parallelRegions", tr.parallel_regions},
                    {"tasksCreated", tr.tasks_created},
                    {"maxThreadsUsed", tr.max_threads_used},
                    {"singleRegions", tr.single_regions},
                    {"taskwaits", tr.taskwaits},
                    {"barriers", tr.barriers},
                    {"criticals", tr.criticals},
                    {"forLoops", tr.for_loops},
                    {"atomics", tr.atomics},
                    {"sections", tr.sections},
                    {"masters", tr.masters},
                    {"ordered", tr.ordered},
                    {"taskgroups", tr.taskgroups},
                    {"simdConstructs", tr.simd_constructs},
                    {"cancels", tr.cancels},
                    {"flushes", tr.flushes},
                    {"taskyields", tr.taskyields}
                };

                // Memory stats sub-object: allocation tracking
                run["memoryStats"] = {
                    {"peakMemoryBytes", tr.peak_memory_bytes},
                    {"allocations", tr.allocation_count},
                    {"deallocations", tr.deallocation_count},
                    {"limitExceeded", tr.memory_limit_exceeded}
                };

                runs.push_back(run);
            }
            test_entry["runs"] = runs;
            tests_grouped.push_back(test_entry);
        }
        scenario["tests"] = tests_grouped;

        // Performance metrics: compare threads=1 vs threads=N
        if(perf_mode && thread_counts.size() >= 2) {
            const auto& result_t1 = results[s];
            const auto& result_tp = results[(thread_counts.size() - 1) * num_scenarios + s];

            double t1_total = 0.0, tp_total = 0.0;
            for(const auto& t : result_t1.results) t1_total += t.time_ms;
            for(const auto& t : result_tp.results) tp_total += t.time_ms;

            int p = thread_counts.back();
            scenario["metrics"] = {
                {"t1Ms", t1_total},
                {"tpMs", tp_total},
                {"speedup", (tp_total > 0.0) ? t1_total / tp_total : 0.0},
                {"efficiency", (tp_total > 0.0 && p > 0) ? (t1_total / tp_total) / p : 0.0}
            };
        }

        scenarios_json.push_back(scenario);
    }

    return scenarios_json;
}

nlohmann::json TestScenarioResultConverter::to_json(const TestScenarioResult& testScenarioResult) {
    nlohmann::json json;

    json["name"] = testScenarioResult.name;
    json["threads"] = testScenarioResult.threads;

    auto tests = nlohmann::json::array();
    for(const auto& test : testScenarioResult.results) {
        tests.push_back(TestResultConverter::to_json(test));
    }
    json["tests"] = tests;

    if(testScenarioResult.numa_node >= 0) {
        json["numaNode"] = testScenarioResult.numa_node;
    }

    return json;
}