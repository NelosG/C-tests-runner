#include <wc_test_common.h>

namespace {

    using wc_common::fixed_input;
    using wc_common::random_input;
    using wc_common::matches_reference;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"single_word",        fixed_input(algo, "hello"),                       matches_reference()},
            {"two_words",          fixed_input(algo, "hello world"),                 matches_reference()},
            {"repeated_words",     fixed_input(algo, "one two one two one"),         matches_reference()},
            {"mixed_case",         fixed_input(algo, "Foo BAR foo BAR"),             matches_reference()},
            {"punctuation",        fixed_input(algo, "hi, world! how-are you?"),     matches_reference()},
            {"only_separators",    fixed_input(algo, "   ,,, !!!  "),                matches_reference()},
            {"random_1k",          random_input(algo, 1'000),                        matches_reference()},
            {"random_10k",         random_input(algo, 10'000),                       matches_reference()},
            {"random_100k",        random_input(algo, 100'000),                      matches_reference()}
        };
    }

} // namespace

class HistogramWordCountsCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("histogram_word_counts");
        }
        std::string name() const override {
            return "Correctness.HistogramWordCounts";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(HistogramWordCountsCorrectness)
