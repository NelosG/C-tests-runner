#include <index_test_common.h>

namespace {

    using index_common::fixed_input;
    using index_common::random_input;
    using index_common::matches_reference;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"two_docs_simple",
             fixed_input(algo, "\nhello world\nfoo bar hello", "\n"),
             matches_reference()},
            {"three_docs_with_repeats",
             fixed_input(algo,
                 "\nalpha beta gamma\nbeta delta\nalpha epsilon beta",
                 "\n"),
             matches_reference()},
            {"mixed_case_punctuation",
             fixed_input(algo,
                 "\nHello, World!\nFoo-Bar; HELLO?\nfoo BAR.",
                 "\n"),
             matches_reference()},
            {"random_10_docs",  random_input(algo, 10,  20),  matches_reference()},
            {"random_50_docs",  random_input(algo, 50,  30),  matches_reference()},
            {"random_200_docs", random_input(algo, 200, 40),  matches_reference()}
        };
    }

} // namespace

class ParallelBuildIndexCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("parallel_build_index");
        }
        std::string name() const override {
            return "Correctness.ParallelBuildIndex";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ParallelBuildIndexCorrectness)
