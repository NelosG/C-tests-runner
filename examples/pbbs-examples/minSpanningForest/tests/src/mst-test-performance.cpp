#include <mst_test_common.h>

namespace {

    using mst_common::random_input;
    using mst_common::has_some_mst;

    // Single large weighted graph. 5M vertices, 30M edges. threads=1
    // baseline ~25-40 s. RAM ~400 MB (weights + edge list).
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_7M_45M", random_input(algo, 7'000'000, 45'000'000), has_some_mst()}
        };
    }

} // namespace

class ParallelKruskalMSTPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_kruskal_mst");
        }
        std::string name() const override {
            return "Performance.ParallelKruskalMST";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelKruskalMSTPerf)
