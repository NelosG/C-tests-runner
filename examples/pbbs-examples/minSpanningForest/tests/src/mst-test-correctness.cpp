#include <mst_test_common.h>

namespace {

    using mst_common::fixed_input;
    using mst_common::random_input;
    using mst_common::check_mst;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single_edge",   fixed_input(algo, 2, {{0,1,1.0f}}),                                check_mst()},
            {"triangle",      fixed_input(algo, 3, {{0,1,1.0f},{1,2,2.0f},{0,2,3.0f}}),          check_mst()},
            {"square_cycle",  fixed_input(algo, 4, {{0,1,1.0f},{1,2,2.0f},{2,3,3.0f},{3,0,4.0f}}), check_mst()},
            {"two_components",fixed_input(algo, 6, {{0,1,1.0f},{1,2,2.0f},{3,4,3.0f},{4,5,1.0f}}), check_mst()},
            {"random_100",    random_input(algo, 100,    300),                                    check_mst()},
            {"random_1k",     random_input(algo, 1000,   3000),                                   check_mst()},
            {"random_10k",    random_input(algo, 10'000, 30'000),                                 check_mst()}
        };
    }

} // namespace

class ParallelKruskalMSTCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_kruskal_mst");
        }
        std::string name() const override {
            return "Correctness.ParallelKruskalMST";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelKruskalMSTCorrectness)
