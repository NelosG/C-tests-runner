#include <bfs_test_common.h>

namespace {

    using bfs_common::random_input;
    using bfs_common::has_size;

    // Single large random graph. 2M vertices, 20M edges is the
    // regime where parlay BFS shows real speedup; per-variant
    // threads=1 baseline lands around 5-10 s, so the three variants
    // together stay under the 200 s per-example budget.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_3M", random_input(algo, 0, 3'000'000, 30'000'000), has_size(3'000'000)}
        };
    }

} // namespace

#define BFS_VARIANT_PERF(ClassName, scenario_label, algo_key)             \
    class ClassName final : public TestScenarioExtension {                \
        public:                                                           \
            std::vector<Test> get_tests() const override {                \
                return tests_for(algo_key);                               \
            }                                                             \
            std::string name() const override {                           \
                return "Performance." scenario_label;                     \
            }                                                             \
            ScenarioType scenario_type() const override {                 \
                return ScenarioType::PERFORMANCE;                         \
            }                                                             \
    };                                                                    \
    REGISTER_TEST(ClassName)

BFS_VARIANT_PERF(SimpleBFSPerf,        "SimpleBFS",        "simple_bfs")
BFS_VARIANT_PERF(DeterministicBFSPerf, "DeterministicBFS", "deterministic_bfs")
BFS_VARIANT_PERF(BackForwardBFSPerf,   "BackForwardBFS",   "back_forward_bfs")
