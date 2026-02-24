#include <omp.h>
#include <test_engine.h>
#include <test_scenario_extension.h>
#include <par/monitor.h>

std::vector<TestScenarioResult> TestEngine::execute(
    const std::vector<int>& thread_counts,
    const TestRegistry& registry,
    par::MonitorContext& ctx,
    const ScenarioType type_filter
) {
    par::monitor::activateContext(&ctx);
    omp_set_dynamic(0);

    const auto& scenarios = registry.all();
    std::vector<TestScenarioResult> results;

    for(const int threads : thread_counts) {
        ctx.max_threads = threads;
        omp_set_num_threads(threads);

        for(const auto& scenario : scenarios) {
            if(scenario->scenario_type() != type_filter) continue;

            auto tests = scenario->getTests();
            std::vector<TestResult> test_results;
            test_results.reserve(tests.size());

            for(const auto& test : tests) {
                ctx.resetStats();

                auto tr = TestScenarioExtension::runTest(test);

                const auto& st = ctx.stats;
                tr.parallel_regions = st.parallel_regions.load(std::memory_order_relaxed);
                tr.tasks_created = st.tasks_created.load(std::memory_order_relaxed);
                tr.max_threads_used = st.max_threads_observed.load(std::memory_order_relaxed);
                tr.single_regions = st.single_regions.load(std::memory_order_relaxed);
                tr.taskwaits = st.taskwaits.load(std::memory_order_relaxed);
                tr.barriers = st.barriers.load(std::memory_order_relaxed);
                tr.criticals = st.criticals.load(std::memory_order_relaxed);
                tr.for_loops = st.for_loops.load(std::memory_order_relaxed);
                tr.atomics = st.atomics.load(std::memory_order_relaxed);
                tr.sections = st.sections.load(std::memory_order_relaxed);
                tr.masters = st.masters.load(std::memory_order_relaxed);
                tr.ordered = st.ordered.load(std::memory_order_relaxed);
                tr.taskgroups = st.taskgroups.load(std::memory_order_relaxed);
                tr.simd_constructs = st.simd_constructs.load(std::memory_order_relaxed);
                tr.cancels = st.cancels.load(std::memory_order_relaxed);
                tr.flushes = st.flushes.load(std::memory_order_relaxed);
                tr.taskyields = st.taskyields.load(std::memory_order_relaxed);
                tr.work_ns = st.work_ns.load(std::memory_order_relaxed);
                tr.span_ns = st.span_ns.load(std::memory_order_relaxed);

                // Use wall-clock span when DAG-based tracking is not applicable:
                // - No tasks → for-based pattern, no DAG to track
                // - No single region → spanInitRoot never called, DAG has no root
                // - span_ns == 0 → defensive fallback (e.g. NORMAL mode)
                {
                    int tasks = st.tasks_created.load(std::memory_order_relaxed);
                    int singles = st.single_regions.load(std::memory_order_relaxed);
                    if(tasks == 0 || singles == 0 || tr.span_ns == 0) {
                        tr.span_ns = static_cast<long long>(tr.time_ms * 1e6);
                    }
                }

                test_results.push_back(std::move(tr));
            }

            TestScenarioResult result(scenario->name(), std::move(test_results), threads);
            results.emplace_back(std::move(result));
        }
    }

    return results;
}