#include <dedup_test_common.h>

namespace {

    using dedup_common::fixed_input;
    using dedup_common::random_input;
    using dedup_common::check_dedup;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single",             fixed_input(algo, {42}),                       check_dedup()},
            {"all_unique",         fixed_input(algo, {1, 2, 3, 4, 5}),            check_dedup()},
            {"all_duplicates",     fixed_input(algo, {7, 7, 7, 7, 7}),            check_dedup()},
            {"mixed",              fixed_input(algo, {1, 2, 1, 3, 2, 4, 3, 5}),   check_dedup()},
            {"random_10k_small_range", random_input(algo, 10'000,    100),         check_dedup()},
            {"random_100k_medium_range", random_input(algo, 100'000, 10'000),     check_dedup()},
            {"random_1M_large_range",    random_input(algo, 1'000'000, 500'000),  check_dedup()}
        };
    }

} // namespace

class ParlayhashDedupCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parlayhash_dedup");
        }
        std::string name() const override {
            return "Correctness.ParlayhashDedup";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParlayhashDedupCorrectness)
