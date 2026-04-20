#include <test_result_converter.h>

nlohmann::json TestResultConverter::parallel_stats_json(const TestResult& tr) {
    return {
        {"parallelRegions", tr.parallel_regions},
        {"tasksCreated", tr.tasks_created},
        {"maxThreadsUsed", tr.max_threads_used},
        {"singleRegions", tr.single_regions},
        {"taskWaits", tr.taskwaits},
        {"barriers", tr.barriers},
        {"criticals", tr.criticals},
        {"forLoops", tr.for_loops},
        {"atomics", tr.atomics},
        {"sections", tr.sections},
        {"masters", tr.masters},
        {"ordered", tr.ordered},
        {"taskGroups", tr.taskgroups},
        {"simdConstructs", tr.simd_constructs},
        {"cancels", tr.cancels},
        {"flushes", tr.flushes},
        {"taskYields", tr.taskyields},
        // Raw nanosecond integers - derived workMs/spanMs/parallelism live in
        // the `stats` block, but those are lossy double conversions. Keep the
        // ints around so the orchestrator can do its own analysis without
        // round-tripping through ms.
        {"workNs", tr.work_ns},
        {"spanNs", tr.span_ns}
    };
}

nlohmann::json TestResultConverter::process_stats_json(const TestResult& tr) {
    return {
        {"exitCode", tr.exit_code},
        {"cgMemPeakKb", tr.cg_mem_peak_kb},
        {"maxRssKb", tr.max_rss_kb},
        {"cpuTimeSec", tr.cpu_time_sec},
        {"wallTimeSec", tr.wall_time_sec},
        {"oomKilled", tr.oom_killed},
        {"timedOut", tr.timed_out}
    };
}
