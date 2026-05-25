#include <bfs_test_common.h>

namespace {

    using bfs_common::fixed_input;
    using bfs_common::random_input;
    using bfs_common::check_bfs;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single_vertex", fixed_input(algo, 0, 1, {}),                                    check_bfs()},
            {"single_edge",   fixed_input(algo, 0, 2, {{0,1}}),                               check_bfs()},
            {"triangle",      fixed_input(algo, 0, 3, {{0,1},{1,2},{0,2}}),                   check_bfs()},
            {"path_5",        fixed_input(algo, 0, 5, {{0,1},{1,2},{2,3},{3,4}}),             check_bfs()},
            {"disconnected",  fixed_input(algo, 0, 6, {{0,1},{1,2},{3,4},{4,5}}),             check_bfs()},
            {"random_100",    random_input(algo, 0, 100,    300),                              check_bfs()},
            {"random_1k",     random_input(algo, 0, 1000,   3000),                             check_bfs()},
            {"random_10k",    random_input(algo, 0, 10'000, 30'000),                           check_bfs()}
        };
    }

} // namespace

#define BFS_VARIANT_SCENARIO(ClassName, scenario_label, algo_key)         \
    class ClassName final : public TestScenarioExtension {                \
        public:                                                           \
            std::vector<Test> get_tests() const override {                \
                return tests_for(algo_key);                               \
            }                                                             \
            std::string name() const override {                           \
                return "Correctness." scenario_label;                     \
            }                                                             \
            ScenarioType scenario_type() const override {                 \
                return ScenarioType::CORRECTNESS;                         \
            }                                                             \
    };                                                                    \
    REGISTER_TEST(ClassName)

BFS_VARIANT_SCENARIO(SimpleBFSCorrectness,        "SimpleBFS",        "simple_bfs")
BFS_VARIANT_SCENARIO(DeterministicBFSCorrectness, "DeterministicBFS", "deterministic_bfs")
BFS_VARIANT_SCENARIO(BackForwardBFSCorrectness,   "BackForwardBFS",   "back_forward_bfs")
