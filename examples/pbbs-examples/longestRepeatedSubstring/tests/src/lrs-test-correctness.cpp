#include <lrs_test_common.h>

namespace {

    using lrs_common::fixed_input;
    using lrs_common::planted_input;
    using lrs_common::random_input;
    using lrs_common::check_lrs;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"banana",        fixed_input(algo, "banana"),                                check_lrs()},
            {"mississippi",   fixed_input(algo, "mississippi"),                           check_lrs()},
            {"planted_short", planted_input(algo, "xyz", "abcdef"),                       check_lrs()},
            {"planted_long",
                              planted_input(algo, "abcdefghij",
                                  "thequickbrownfoxjumpsoverthelazydog"),                  check_lrs()},
            {"random_1k",     random_input(algo, 1'000),                                  check_lrs()},
            {"random_10k",    random_input(algo, 10'000),                                 check_lrs()},
            {"random_100k",   random_input(algo, 100'000),                                check_lrs()}
        };
    }

} // namespace

class DoublingLRSCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("doubling_lrs");
        }
        std::string name() const override {
            return "Correctness.DoublingLRS";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(DoublingLRSCorrectness)
