// Performance scenarios per variant. Engine drives the thread-count sweep
// and produces a scalability summary. Sizes are deliberately large so the
// parallel speedup is observable.

#include <sort_test_common.h>

namespace {

    using sort_common::random_input;
    using sort_common::has_size;

    // One large scenario per variant; the engine still sweeps the
    // five thread counts inside it. 50M doubles ~ 400MB input plus
    // sample-sort aux buffers, well within 8 GB; threads=1 baseline
    // lands around 25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_200M", random_input(algo, 200'000'000), has_size(200'000'000)}
        };
    }

} // namespace

#define SORT_VARIANT_PERF(ClassName, scenario_label, algo_key)            \
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

SORT_VARIANT_PERF(ParlaySampleSortPerf,    "ParlaySampleSort",    "parlay_sample_sort")
SORT_VARIANT_PERF(QuickSortPerf,           "QuickSort",           "quick_sort")
SORT_VARIANT_PERF(MergeSortPerf,           "MergeSort",           "merge_sort")
SORT_VARIANT_PERF(StableSampleSortPerf,    "StableSampleSort",    "stable_sample_sort")
