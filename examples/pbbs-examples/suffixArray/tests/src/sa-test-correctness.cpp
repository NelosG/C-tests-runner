#include <sa_test_common.h>

namespace {

    using sa_common::fixed_input;
    using sa_common::random_input;
    using sa_common::check_sa;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"banana",       fixed_input(algo, "banana"),                    check_sa()},
            {"mississippi",  fixed_input(algo, "mississippi"),               check_sa()},
            {"all_same",     fixed_input(algo, "aaaaaa"),                    check_sa()},
            {"alphabet",     fixed_input(algo, "abcdefghijklmnopqrstuvwxyz"),check_sa()},
            {"random_1k",    random_input(algo, 1'000),                      check_sa()},
            {"random_10k",   random_input(algo, 10'000),                     check_sa()},
            {"random_100k",  random_input(algo, 100'000),                    check_sa()}
        };
    }

} // namespace

class ParallelKSCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_ks");
        }
        std::string name() const override {
            return "Correctness.ParallelKS";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelKSCorrectness)
