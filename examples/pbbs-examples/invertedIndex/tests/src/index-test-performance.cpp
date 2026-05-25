#include <index_test_common.h>

namespace {

    using index_common::random_input;
    using index_common::has_some_output;

    // 50k docs put threads=1 baseline near 160 s on WSL (parlay hash
    // table fills + sort dominate); 15k keeps it under 60 s while
    // still exercising the parallel reduce path.
    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"perf_15k_docs", random_input(algo, 15'000, 500), has_some_output()}
        };
    }

} // namespace

class ParallelBuildIndexPerf final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_build_index");
        }
        std::string name() const override {
            return "Performance.ParallelBuildIndex";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::PERFORMANCE;
        }
};
REGISTER_TEST(ParallelBuildIndexPerf)
