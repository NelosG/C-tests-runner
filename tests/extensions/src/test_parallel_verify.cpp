#include <algorithm>
#include <quick_sort.h>
#include <random>
#include <register_test.h>
#include <test_scenario_extension.h>
#include <vector>
#include <omp.h>
#include <par/monitor.h>

/**
 * @brief Verifies that the student solution actually uses parallel constructs.
 *
 * Runs the solution under par::monitor MONITOR mode and checks that
 * parallel regions, tasks, etc. were created. Also runs a stress pass
 * with injected delays to expose potential race conditions.
 */
class ParallelVerifyTest final : public TestScenarioExtension {

    std::vector<Test> getTests() const override {
        auto data = std::make_shared<std::vector<int>>();
        auto stats_snap = std::make_shared<par::Stats>();

        auto stress_data = std::make_shared<std::vector<int>>();
        auto stress_ref = std::make_shared<std::vector<int>>();

        return {
            // Test 1: Verify that parallel constructs were used
            Test{
                "uses_parallel_constructs",
                // Setup: generate data and enable monitoring
                [data]() {
                    std::mt19937 gen(55555);
                    data->resize(50000);
                    for(auto& x : *data) x = static_cast<int>(gen());
                    // Ensure MONITOR mode is active (TestEngine sets this,
                    // but we explicitly set it for standalone use)
                    par::monitor::setMode(par::Mode::MONITOR);
                    par::monitor::resetStats();
                },
                // Execute: run the student's sort
                [data]() {
                    parallel::qsort(*data);
                },
                // Verify: check that parallel constructs were used
                [data]() -> std::pair<bool, std::string> {
                    const auto& st = par::monitor::stats();
                    int regions = st.parallel_regions.load(std::memory_order_relaxed);
                    int tasks = st.tasks_created.load(std::memory_order_relaxed);
                    int fors = st.for_loops.load(std::memory_order_relaxed);

                    if(!std::is_sorted(data->begin(), data->end()))
                        return {false, "Array not sorted"};

                    // Must have at least one parallel region
                    if(regions == 0)
                        return {false, "No parallel regions used (par::parallel() never called)"};

                    // Must use either tasks or parallel for
                    if(tasks == 0 && fors == 0)
                        return {
                            false,
                            "No parallel work created "
                            "(neither par::task() nor par::parallel_for() called)"
                        };

                    return {
                        true,
                        "parallel_regions=" + std::to_string(regions)
                        + " tasks=" + std::to_string(tasks)
                        + " for_loops=" + std::to_string(fors)
                    };
                }
            },

            // Test 2: Stress test — inject random delays to expose race conditions
            Test{
                "stress_test_races",
                // Setup: generate data + reference sort, enable STRESS mode
                [stress_data, stress_ref]() {
                    std::mt19937 gen(77777);
                    stress_data->resize(10000);
                    for(auto& x : *stress_data) x = static_cast<int>(gen());
                    // Compute reference result
                    *stress_ref = *stress_data;
                    std::sort(stress_ref->begin(), stress_ref->end());
                    // Enable stress mode with random delays
                    par::monitor::setMode(par::Mode::STRESS);
                    par::monitor::resetStats();
                },
                // Execute: sort under stress
                [stress_data]() {
                    parallel::qsort(*stress_data);
                },
                // Verify: result must match reference
                [stress_data, stress_ref]() -> std::pair<bool, std::string> {
                    if(*stress_data != *stress_ref)
                        return {
                            false,
                            "Sort produced incorrect result under stress "
                            "(possible race condition)"
                        };
                    return {true, ""};
                }
            },

            // Test 3: Multiple threads were actually used
            Test{
                "uses_multiple_threads",
                [data]() {
                    std::mt19937 gen(88888);
                    data->resize(100000);
                    for(auto& x : *data) x = static_cast<int>(gen());
                    par::monitor::setMode(par::Mode::MONITOR);
                    par::monitor::resetStats();
                },
                [data]() {
                    parallel::qsort(*data);
                },
                [data]() -> std::pair<bool, std::string> {
                    if(!std::is_sorted(data->begin(), data->end()))
                        return {false, "Array not sorted"};

                    const auto& st = par::monitor::stats();
                    int max_t = st.max_threads_observed.load(std::memory_order_relaxed);
                    int configured = omp_get_max_threads();

                    // Only fail if the engine configured >1 thread but solution used just 1
                    if(configured > 1 && max_t <= 1)
                        return {false, "Only 1 thread used — solution is not parallel"};

                    return {true, "max_threads=" + std::to_string(max_t)};
                }
            }
        };
    }

    public:
        std::string name() const override { return "Correctness.ParallelVerify"; }
        ~ParallelVerifyTest() override = default;
};

REGISTER_TEST(ParallelVerifyTest)