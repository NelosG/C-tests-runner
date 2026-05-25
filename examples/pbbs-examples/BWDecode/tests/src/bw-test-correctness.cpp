#include <bw_test_common.h>

namespace {

    using bw_common::fixed_input;
    using bw_common::random_input;
    using bw_common::matches_plaintext;

    std::vector<Test> tests_for(const std::string& algo) {
        return {
            {"short_text",           fixed_input(algo, "banana"),                 matches_plaintext()},
            {"single_char",          fixed_input(algo, "a"),                      matches_plaintext()},
            {"all_same",             fixed_input(algo, "aaaaaaa"),                matches_plaintext()},
            {"pangram",              fixed_input(algo, "the quick brown fox jumps over the lazy dog"), matches_plaintext()},
            {"repeated_pattern",     fixed_input(algo, "abcabcabcabcabcabcabc"),  matches_plaintext()},
            {"random_1k",            random_input(algo, 1'000),                   matches_plaintext()},
            {"random_10k",           random_input(algo, 10'000),                  matches_plaintext()},
            {"random_100k",          random_input(algo, 100'000),                 matches_plaintext()}
        };
    }

} // namespace

class ListRankBWDecodeCorrectness final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return tests_for("list_rank_bw_decode");
        }
        std::string name() const override {
            return "Correctness.ListRankBWDecode";
        }
        ScenarioType scenario_type() const override {
            return ScenarioType::CORRECTNESS;
        }
};
REGISTER_TEST(ListRankBWDecodeCorrectness)
