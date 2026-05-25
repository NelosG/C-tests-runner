#include <isort_test_common.h>

namespace {

    using isort_common::random_input;
    using isort_common::has_size;

    // Single large scenario. 200M 32-bit keys (4 radix passes, matching
    // pbbs's randomSeq int range) ~ 800 MB input plus aux buffers.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_200M_32b", random_input(algo, 200'000'000, 32), has_size(200'000'000)}
        };
    }

} // namespace

class ParallelRadixSortPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_radix_sort");
        }
        std::string name() const override {
            return "Performance.ParallelRadixSort";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelRadixSortPerf)
