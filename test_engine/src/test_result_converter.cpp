#include <test_result_converter.h>

nlohmann::json TestResultConverter::to_json(const TestResult& testResult) {
    nlohmann::json json;

    json["name"] = testResult.name;
    json["passed"] = testResult.passed;
    if(!testResult.message.empty()) {
        json["message"] = testResult.message;
    }

    json["stats"] = {
        {"timeMs", testResult.time_ms},
        {"workMs", static_cast<double>(testResult.work_ns) / 1e6},
        {"spanMs", static_cast<double>(testResult.span_ns) / 1e6},
        {
            "parallelism",
            (testResult.span_ns > 0)
            ? static_cast<double>(testResult.work_ns) / static_cast<double>(testResult.span_ns)
            : 0.0
        }
    };

    json["parallelStats"] = {
        {"parallelRegions", testResult.parallel_regions},
        {"tasksCreated", testResult.tasks_created},
        {"maxThreadsUsed", testResult.max_threads_used},
        {"singleRegions", testResult.single_regions},
        {"taskwaits", testResult.taskwaits},
        {"barriers", testResult.barriers},
        {"criticals", testResult.criticals},
        {"forLoops", testResult.for_loops},
        {"atomics", testResult.atomics},
        {"sections", testResult.sections},
        {"masters", testResult.masters},
        {"ordered", testResult.ordered},
        {"taskgroups", testResult.taskgroups},
        {"simdConstructs", testResult.simd_constructs},
        {"cancels", testResult.cancels},
        {"flushes", testResult.flushes},
        {"taskyields", testResult.taskyields}
    };

    json["memoryStats"] = {
        {"peakMemoryBytes", testResult.peak_memory_bytes},
        {"allocations", testResult.allocation_count},
        {"deallocations", testResult.deallocation_count},
        {"limitExceeded", testResult.memory_limit_exceeded}
    };

    return json;
}