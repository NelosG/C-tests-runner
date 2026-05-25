// Performance scenarios. Sizes are large enough that parallel speedup is
// visible on multi-core runs.

#include <histogram_test_common.h>

namespace {

    using histogram_common::random_input;
    using histogram_common::has_size;

    // Single large scenario. 500M ints (2 GB input) with 4096
    // buckets - memory-bandwidth dominated; threads=1 baseline
    // around 25-40 s on this size.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_200M_1Mbuckets", random_input(algo, 200'000'000, 1'000'000), has_size(1'000'000)}
        };
    }

} // namespace

class ParallelHistogramPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_histogram");
        }
        std::string name() const override {
            return "Performance.ParallelHistogram";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelHistogramPerf)
