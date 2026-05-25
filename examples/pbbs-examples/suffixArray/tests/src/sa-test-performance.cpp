#include <sa_test_common.h>

namespace {

    using sa_common::random_input;
    using sa_common::has_size;

    // Single large scenario. Parallel-KS suffix array on 20M chars
    // (~1 GB working set with aux arrays); threads=1 baseline ~25-40 s.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_30M", random_input(algo, 30'000'000), has_size(30'000'000)}
        };
    }

} // namespace

class ParallelKSPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_ks");
        }
        std::string name() const override {
            return "Performance.ParallelKS";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelKSPerf)
